#include "dialog_speech.h"
#include "hd_hooks.h"
#include <cmath>
#include <cstdio>
#include <cstdarg>

namespace disco_dialogues {
namespace {
FILE* speechLog = nullptr;
unsigned speechLogLines = 0;
void SpeechTrace(const char* format, ...) {
    if (!speechLog || speechLogLines++ >= 10000) return;
    std::fprintf(speechLog, "%lu ", ::GetTickCount());
    va_list args;
    va_start(args, format);
    std::vfprintf(speechLog, format, args);
    va_end(args);
    std::fputc('\n', speechLog);
    std::fflush(speechLog);
}
using EventFn = int(__thiscall*)(void*, unsigned, unsigned, const void*);
using NativeFn = bool(__thiscall*)(void*, void*&, void**, unsigned, void*);
using ResumeFn = void(__thiscall*)(void*, unsigned, unsigned);
EventFn originalEvent = nullptr;
NativeFn originalStop = nullptr;
NativeFn originalPlay = nullptr;
ResumeFn originalResume = nullptr;
void** speechStreamTable = nullptr;
void** speechBufferTable = nullptr;

struct SoundEntry { void* sound; float volume; unsigned flags; };
static_assert(sizeof(SoundEntry) == 12);

void __fastcall ResumeSounds(void* self, void*, unsigned flags, unsigned mask) {
    if (speechLog) SpeechTrace("resume collection=%p flags=%x mask=%x enabled=%d", self, flags, mask, ShouldPreserveDialogSpeech());
    // World::RestoreAfterLengthyStop resumes the collection on dialogue exit.
    // Speech created during that stop was never paused. HD Stream::Resume
    // decrements zero to -1 and calls alSourcePlay, restarting the current voice.
    if (flags != 0 || mask != 2 || !ShouldPreserveDialogSpeech()) {
        originalResume(self, flags, mask);
        return;
    }
    auto bytes = static_cast<char*>(self);
    // HD has separate vectors for ISoundInst and ISound3DInst. Preserve its
    // flag matching and resume semantics except for unpaused 3D voice streams.
    for (unsigned dimension = 0; dimension < 2; ++dimension) {
        const unsigned offset = dimension ? 0x14 : 8;
        auto entry = *reinterpret_cast<SoundEntry**>(bytes + offset);
        auto end = *reinterpret_cast<SoundEntry**>(bytes + offset + 4);
        for (; entry != end; ++entry) {
            if ((entry->flags ^ flags) & mask) continue;
            const auto table = *static_cast<void***>(entry->sound);
            SpeechTrace("resume entry=%p 3d=%u flags=%x stream=%d buffered=%d", entry->sound, dimension, entry->flags,
                table == speechStreamTable, table == speechBufferTable);
            if (dimension && (entry->flags & 0x10000) &&
                (table == speechStreamTable || table == speechBufferTable) &&
                hd::Method<int(__thiscall*)(void*)>(entry->sound, 20)(entry->sound) <= 0) {
                SpeechTrace("resume skip=%p", entry->sound);
                continue;
            }
            hd::Method<void(__thiscall*)(void*)>(entry->sound, dimension ? 9 : 8)(entry->sound);
        }
    }
}

struct ReplyScope {
    void* script;
    void* preservedActor = nullptr;
};
thread_local ReplyScope* replyScope = nullptr;

// Engine HD SendEvent executes the handler synchronously with Run(0). Scope
// EVERY event: a nested unload/dispose/other event must regain vanilla cleanup.
// No event, receiver, actor or audio handle is retained beyond this call.
int __fastcall SendEvent(void* script, void*, unsigned event, unsigned count, const void* vars) {
    if (event == 0 || event == 11) SpeechTrace("event enter script=%p event=%u count=%u", script, event, count);
    ReplyScope scope{event == 11 && count == 2 && ShouldPreserveDialogSpeech() ? script : nullptr};
    struct Restore {
        ReplyScope* previous;
        ~Restore() { replyScope = previous; }
    } restore{replyScope};
    replyScope = &scope;
    const int result = originalEvent(script, event, count, vars);
    if (event == 0 || event == 11) SpeechTrace("event leave script=%p event=%u", script, event);
    return result;
}

void TraceNative(const char* name, void* self, unsigned count) {
    if (!speechLog) return;
    __try {
        auto actor = *reinterpret_cast<char**>(static_cast<char*>(self) + 4);
        SpeechTrace("%s actor=%p count=%u frame=%.4f end=%.4f scope=%p", name, actor, count,
            *reinterpret_cast<float*>(actor + 0x330), *reinterpret_cast<float*>(actor + 0x334),
            replyScope ? replyScope->script : nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) { SpeechTrace("%s unreadable actor", name); }
}

bool SpeakingInReply(void* function, void*& actor) {
    if (!replyScope || !replyScope->script) return false;
    __try {
        actor = *reinterpret_cast<void**>(static_cast<char*>(function) + 4);
        if (!actor) return false;
        const auto bytes = static_cast<const char*>(actor);
        // Verified CActorBipedLSH HD layout. Do not intercept speech owned by
        // another actor, an unloaded actor, or a latent speech waiter.
        if (!bytes[0x24] || !*reinterpret_cast<void* const*>(bytes + 0x300) ||
            *reinterpret_cast<void* const*>(bytes + 0x32c)) return false;
        // +0x16c holds CCABTasked, not IScript. Its native GetScript resolves
        // the current task through ITaskScripted (including absent tasks).
        auto controller = *reinterpret_cast<void* const*>(bytes + 0x16c);
        if (!controller || hd::Method<void*(__thiscall*)(void*)>(controller, 3)(controller) != replyScope->script)
            return false;
        const float current = *reinterpret_cast<const float*>(bytes + 0x330);
        const float end = *reinterpret_cast<const float*>(bytes + 0x334);
        return std::isfinite(current) && std::isfinite(end) && current >= 0 && current < end;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool __fastcall StopSpeech(void* self, void*, void*& instance, void** args, unsigned count, void* result) {
    TraceNative("stop", self, count);
    void* actor = nullptr;
    if (count == 0 && SpeakingInReply(self, actor)) {
        replyScope->preservedActor = actor;
        SpeechTrace("stop preserved=%p", actor);
        return true;
    }
    return originalStop(self, instance, args, count, result);
}

bool __fastcall PlaySpeech(void* self, void*, void*& instance, void** args, unsigned count, void* result) {
    TraceNative("play", self, count);
    void* actor = nullptr;
    if (count == 1 && SpeakingInReply(self, actor) && replyScope->preservedActor == actor) {
        // A replacement requested by this same reply would erase the speech we
        // just preserved. Drop that request; never queue audio or delay a branch.
        return true;
    }
    return originalPlay(self, instance, args, count, result);
}
}

void InitializeDialogSpeechTrace(const wchar_t* path) {
    if (!speechLog) _wfopen_s(&speechLog, path, L"w");
    SpeechTrace("Disco Dialogues 1.0.0 diagnostic");
}

bool InstallDialogSpeech() {
    SpeechTrace("install speech begin");
    const auto game = ::GetModuleHandleW(nullptr);
    const auto engine = ::GetModuleHandleW(L"Engine.dll");
    const auto sound = ::GetModuleHandleW(L"Sound.dll");
    if (!hd::Module(game, 0x5698d115, 0x4c4000) || !hd::Module(engine, 0x561b8394, 0x26e000) ||
        !hd::Module(sound, 0x565374bc, 0xc6000)) return false;
    const auto g = reinterpret_cast<BYTE*>(game);
    const auto e = reinterpret_cast<BYTE*>(engine);
    const auto s = reinterpret_cast<BYTE*>(sound);
    // CMScriptRun::SendEvent -> CScriptRun::SendEvent -> synchronous Run(0).
    // Native Execute wrappers, actor controller, GetScript, speech state.
    if (!hd::Bytes(e, 0xbfbe0, "\x53\x8b\x5c\x24\x0c\x55\x8b\x6c\x24\x0c\x56\x57\x8b\xf9\x8b\x77\x08\x3b\x77\x0c\x74\x18\x8b\x46\x08\x33\xc5\x85\xc3\x75\x07\x8b\x0e\x8b\x01\xff\x50\x20\x83\xc6\x0c\x3b\x77\x0c\x75\xe8\x8b\x77\x14\x3b\x77\x18\x74\x18\x8b\x46\x08\x33\xc5\x85\xc3\x75\x07\x8b\x0e\x8b\x01\xff\x50\x24\x83\xc6\x0c\x3b\x77\x18\x75\xe8\x5f\x5e\x5d\x5b\xc2\x08\x00", 85) ||
        !hd::Bytes(e, 0x4d59, "\xe9\x82\xae\x0b\x00", 5) ||
        !hd::Bytes(e, 0xea7bb, "\x6a\x02\x6a\x00\x8b\x01\xff\x50\x24", 9) ||
        !hd::Bytes(s, 0x29e90, "\x8b\x49\x14\xe9\x98\x3b\x01\x00", 8) ||
        !hd::Bytes(s, 0x3da30, "\x8b\x41\x6c\xc3", 4) ||
        !hd::Bytes(s, 0x28970, "\x83\xc1\x08\xe9\xd8\x57\x01\x00", 8) ||
        !hd::Bytes(s, 0x3e150, "\x8b\x41\x14\xc3", 4) ||
        !hd::Bytes(e, 0xb47cd, "\x8b\x49\x0c\xff\x75\x08", 6) ||
        !hd::Bytes(e, 0xb47da, "\xe8\x71\xd8\x07\x00", 5) ||
        !hd::Bytes(e, 0x132220, "\xc7\x04\x24\x00\x00\x00\x00\xe8\x24\xfa\xff\xff", 12) ||
        !hd::Bytes(g, 0x5b374, "\x8b\xbe\x6c\x01\x00\x00", 6) ||
        !hd::Bytes(g, 0x18b65, "\xe9\xe6\x85\x13\x00", 5) ||
        !hd::Bytes(g, 0x151150, "\x8b\x49\x0c\x85\xc9\x74\x15\x8b\x01\x68\x2b\x11\x43\x34\xff\x50\x08\x85\xc0\x74\x07\x8b\x10\x8b\xc8\xff\x62\x0c\x33\xc0\xc3", 31) ||
        !hd::Bytes(g, 0x26ae30, "\x8b\x41\x08\xc3", 4) ||
        !hd::Bytes(g, 0x832b0, "\xff\x74\x24\x10\x8b\x49\x04", 7) ||
        !hd::Bytes(g, 0x832bf, "\xe8\x5b\xad\xf9\xff\xc2\x10\x00", 8) ||
        !hd::Bytes(g, 0x83270, "\xff\x74\x24\x10\x8b\x49\x04", 7) ||
        !hd::Bytes(g, 0x8327f, "\xe8\xc6\xfb\xf8\xff\xc2\x10\x00", 8) ||
        !hd::Bytes(g, 0x86fe3, "\x80\x7f\x24\x00", 4) ||
        !hd::Bytes(g, 0x86fe9, "\x8b\xb7\x2c\x03\x00\x00", 6) ||
        !hd::Bytes(g, 0x87029, "\x8b\x8f\x00\x03\x00\x00", 6) ||
        !hd::Bytes(g, 0x84e8d, "\xf3\x0f\x10\x87\x30\x03\x00\x00\xf3\x0f\x10\x8f\x34\x03\x00\x00", 16)) return false;
    if (*reinterpret_cast<void**>(g + 0x371c44) != g + 0x18b65) return false;
    if (*reinterpret_cast<void**>(s + 0x8f970 + 20 * 4) != s + 0x29e90) return false;
    if (*reinterpret_cast<void**>(s + 0x8f89c + 20 * 4) != s + 0x28970) return false;
    speechStreamTable = reinterpret_cast<void**>(s + 0x8f970);
    speechBufferTable = reinterpret_cast<void**>(s + 0x8f89c);
    hd::Slot slots[] = {
        {reinterpret_cast<void**>(e + 0x200534), e + 0x8396, reinterpret_cast<void*>(&SendEvent)},
        {reinterpret_cast<void**>(g + 0x36af60), g + 0x243d4, reinterpret_cast<void*>(&StopSpeech)},
        {reinterpret_cast<void**>(g + 0x36af30), g + 0x2133c, reinterpret_cast<void*>(&PlaySpeech)},
        {reinterpret_cast<void**>(e + 0x20095c), e + 0x4d59, reinterpret_cast<void*>(&ResumeSounds)},
    };
    for (const auto& slot : slots) if (*slot.address != slot.expected) return false;
    originalEvent = reinterpret_cast<EventFn>(slots[0].expected);
    originalStop = reinterpret_cast<NativeFn>(slots[1].expected);
    originalPlay = reinterpret_cast<NativeFn>(slots[2].expected);
    originalResume = reinterpret_cast<ResumeFn>(slots[3].expected);
    for (unsigned i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
        if (hd::Replace(slots[i])) continue;
        while (i > 0) hd::Replace(slots[--i], true);
        return false;
    }
    SpeechTrace("install speech success");
    return true;
}
}
