#include "dialog_policy.h"
#include "OynonRuntimeApi.h"
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
struct ReplyScope { void* script; void* preservedActor = nullptr; };
thread_local ReplyScope* replyScope = nullptr;

int __stdcall Event(const OynonScriptEventCall* call, void*) {
    if (call->event == 0 || call->event == 11)
        SpeechTrace("event enter script=%p event=%u count=%u", call->script, call->event, call->count);
    ReplyScope scope{call->event == 11 && call->count == 2 && ShouldPreserveDialogSpeech() ? call->script : nullptr};
    struct Restore { ReplyScope* previous; ~Restore() { replyScope = previous; } } restore{replyScope};
    replyScope = &scope;
    const int result = OynonProceedScriptEvent(call);
    if (call->event == 0 || call->event == 11)
        SpeechTrace("event leave script=%p event=%u", call->script, call->event);
    return result;
}
bool SpeakingInReply(const OynonSpeechCall* call, void*& actor) {
    if (!replyScope || !replyScope->script) return false;
    OynonActorSpeechState state{};
    if (!OynonReadActorSpeech(call, &state)) return false;
    actor = state.actor;
    if (!actor || !state.loaded || !state.hasHead || state.hasWaiter) return false;
    void* script = nullptr;
    if (!OynonReadActorScript(call, &script) || script != replyScope->script) return false;
    return std::isfinite(state.current) && std::isfinite(state.end) && state.current >= 0 && state.current < state.end;
}
BOOL __stdcall Speech(const OynonSpeechCall* call, void*) {
    const bool stop = call->operation == OYNON_SPEECH_STOP;
    if (speechLog) {
        OynonActorSpeechState state{};
        const char* name = stop ? "stop" : "play";
        if (OynonReadActorSpeech(call, &state))
            SpeechTrace("%s actor=%p count=%u frame=%.4f end=%.4f scope=%p", name, state.actor, call->count,
                state.current, state.end, replyScope ? replyScope->script : nullptr);
        else SpeechTrace("%s unreadable actor", name);
    }
    void* actor = nullptr;
    if (call->count == (stop ? 0u : 1u) && SpeakingInReply(call, actor)) {
        if (stop) {
            replyScope->preservedActor = actor;
            SpeechTrace("stop preserved=%p", actor);
            return TRUE;
        }
        if (replyScope->preservedActor == actor) return TRUE;
    }
    return OynonProceedSpeech(call);
}
BOOL __stdcall ResumeEntry(const OynonSoundEntry* entry, void*) {
    SpeechTrace("resume entry=%p 3d=%u flags=%x stream=%d buffered=%d", entry->sound,
        static_cast<unsigned>(entry->is3D), entry->flags, entry->isStream, entry->isBuffer);
    int paused = 0;
    if (entry->is3D && (entry->flags & 0x10000) && (entry->isStream || entry->isBuffer) &&
        OynonReadSoundPauseCount(entry, &paused) && paused <= 0) {
        SpeechTrace("resume skip=%p", entry->sound);
        return FALSE;
    }
    return TRUE;
}
void __stdcall Resume(const OynonSoundResumeCall* call, void*) {
    if (speechLog) SpeechTrace("resume collection=%p flags=%x mask=%x enabled=%d",
        call->collection, call->flags, call->mask, ShouldPreserveDialogSpeech());
    const bool filter = call->flags == 0 && call->mask == 2 && ShouldPreserveDialogSpeech();
    OynonProceedSoundResume(call, filter ? ResumeEntry : nullptr, nullptr);
}
}
void InitializeDialogSpeechTrace(const wchar_t* path) {
    if (!speechLog) _wfopen_s(&speechLog, path, L"w");
    SpeechTrace("Disco Dialogues 1.0.0 diagnostic");
}
bool InstallDialogSpeech() {
    SpeechTrace("install speech begin");
    const OynonScriptAudioCallbacks callbacks{Event, Speech, Resume};
    if (!OynonInstallScriptAudioHooks(&callbacks, nullptr)) return false;
    SpeechTrace("install speech success");
    return true;
}
}
