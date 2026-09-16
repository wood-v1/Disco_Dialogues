// Exercise the actual x86 hook entry points, with native transports substituted.
#include "script_audio_hooks.cpp"
#include "../src/dialog_speech.cpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <fstream>
#include <cstddef>
#include <limits>
#include <stdexcept>

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::printf("FAIL line %d: %s\n", __LINE__, #expr); \
            std::fflush(stdout); \
            std::abort(); \
        } \
    } while (0)

namespace disco_dialogues {
bool enabled = true;

bool ShouldPreserveDialogSpeech() {
    return enabled;
}
}

using namespace oynon::runtime;
using disco_dialogues::enabled;
using disco_dialogues::replyScope;

// HD uses a controller -> task -> ITaskScripted -> IScript chain. In particular,
// the controller stored on the actor must never equal the event's script.
void* taskTable[3]{};
void* scriptedTable[4]{};
void* controllerTable[4]{};

struct Task {
    void** table = taskTable;
    void** scripted = scriptedTable;
    void* serialization = nullptr;
    void* script = nullptr;
    bool supportsScript = true;
};

struct Controller {
    void** table = controllerTable;
    void* serialization = nullptr;
    void* actor = nullptr;
    Task* task = nullptr;
};

static_assert(offsetof(Task, script) == 0xc && offsetof(Task, scripted) == 4);
static_assert(offsetof(Controller, task) == 0xc);

void* __fastcall QueryTask(Task* task, void*, unsigned aid) {
    CHECK(aid == 0x3443112b);
    return task->supportsScript ? &task->scripted : nullptr;
}

void* __fastcall TaskScript(void* scripted, void*) {
    return *reinterpret_cast<void**>(static_cast<char*>(scripted) + 8);
}

void* __fastcall ControllerScript(Controller* controller, void*) {
    if (!controller->task)
        return nullptr;

    auto scripted =
        hd::Method<void*(__thiscall*)(void*, unsigned)>(controller->task, 2)(controller->task, 0x3443112b);
    return scripted ? hd::Method<void*(__thiscall*)(void*)>(scripted, 3)(scripted) : nullptr;
}

struct Actor {
    alignas(16) std::array<unsigned char, 0x370> bytes{};
    Task task;
    Controller controller;

    template <class T>
    void Set(size_t offset, T value) {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }

    template <class T>
    T Get(size_t offset) const {
        T value;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }

    void Start(void* script) {
        bytes.fill(0);
        task.script = script;
        task.supportsScript = true;
        controller.task = &task;
        controller.actor = this;
        Set<void*>(0x16c, &controller);
        Set<unsigned char>(0x24, 1);
        Set<void*>(0x300, this); // Present head; never dereferenced by the hook.
        Set<float>(0x330, 1.25f);
        Set<float>(0x334, 7.0f);
    }
};

struct Native {
    void* table = nullptr;
    void* actor;
};

int stopCalls = 0, playCalls = 0, branchCalls = 0;
void* instance = reinterpret_cast<void*>(0x1234);
void* arguments[] = {reinterpret_cast<void*>(0x2345)};
void* result = reinterpret_cast<void*>(0x3456);
std::function<void(void*, unsigned, unsigned)> handler;

bool __fastcall NativeStop(void* self, void*, void*& inst, void** args, unsigned, void* res) {
    CHECK(&inst == &instance && args == arguments && res == result);
    auto actor = static_cast<Actor*>(static_cast<Native*>(self)->actor);
    actor->Set<float>(0x330, 0);
    actor->Set<float>(0x334, 0);
    ++stopCalls;
    return false; // Ensure passthrough preserves the original return value.
}

bool __fastcall NativePlay(void* self, void*, void*& inst, void** args, unsigned, void* res) {
    CHECK(&inst == &instance && args == arguments && res == result);
    auto actor = static_cast<Actor*>(static_cast<Native*>(self)->actor);
    actor->Set<float>(0x330, 0);
    actor->Set<float>(0x334, 5);
    ++playCalls;
    return false;
}

int __fastcall Dispatch(void* script, void*, unsigned event, unsigned count, const void* vars) {
    CHECK(vars == arguments);
    ++branchCalls;
    handler(script, event, count);
    return 37;
}

bool Stop(Native& native, unsigned count = 0) {
    return StopSpeech(&native, nullptr, instance, arguments, count, result);
}

bool Play(Native& native, unsigned count = 1) {
    return PlaySpeech(&native, nullptr, instance, arguments, count, result);
}

void Event(void* script, unsigned event = 11, unsigned count = 2) {
    CHECK(SendEvent(script, nullptr, event, count, arguments) == 37);
}

struct Sound {
    void** table;
    int paused = 0;
    int resumes = 0;
    float position = 3;
};

void __fastcall ResumeSound(Sound* sound, void*) {
    ++sound->resumes;
    // An OpenAL source already playing restarts on alSourcePlay; a paused
    // source continues. HD CStream::Resume calls it even when unpaused.
    if (sound->paused <= 0)
        sound->position = 0;
    --sound->paused;
}

int __fastcall PauseState(Sound* sound, void*) {
    return sound->paused;
}

struct Collection {
    void* table = nullptr;
    void* engine = nullptr;
    SoundEntry* begin2d = nullptr;
    SoundEntry* end2d = nullptr;
    SoundEntry* capacity2d = nullptr;
    SoundEntry* begin3d = nullptr;
    SoundEntry* end3d = nullptr;
    SoundEntry* capacity3d = nullptr;
};

static_assert(offsetof(Collection, begin2d) == 8 && offsetof(Collection, begin3d) == 0x14);

void __fastcall NativeResume(Collection* collection, void*, unsigned flags, unsigned mask) {
    for (auto entry = collection->begin2d; entry != collection->end2d; ++entry)
        if (!((entry->flags ^ flags) & mask))
            hd::Method<void(__thiscall*)(void*)>(entry->sound, 8)(entry->sound);
    for (auto entry = collection->begin3d; entry != collection->end3d; ++entry)
        if (!((entry->flags ^ flags) & mask))
            hd::Method<void(__thiscall*)(void*)>(entry->sound, 9)(entry->sound);
}

void TestExitResume(bool buffered) {
    void* streamTable[21]{};
    streamTable[8] = streamTable[9] = reinterpret_cast<void*>(&ResumeSound);
    streamTable[20] = reinterpret_cast<void*>(&PauseState);

    void* unrelatedTable[21]{};
    std::memcpy(unrelatedTable, streamTable, sizeof(streamTable));

    void* bufferTable[21]{};
    std::memcpy(bufferTable, streamTable, sizeof(streamTable));

    speechStreamTable = streamTable;
    speechBufferTable = bufferTable;
    auto voiceTable = buffered ? bufferTable : streamTable;
    Sound voice{voiceTable}, ambient{streamTable}, ui{streamTable}, other{unrelatedTable};
    SoundEntry entries[] = {{&voice, 1, 0x10000}, {&ambient, 1, 0}, {&other, 1, 0x10000}};
    SoundEntry entry2d{&ui, 1, 0};
    Collection collection;
    collection.begin2d = &entry2d;
    collection.end2d = &entry2d + 1;
    collection.begin3d = entries;
    collection.end3d = entries + 3;

    // Regression reproduced with native Engine.dll code when ABI fixtures are
    // supplied: vanilla world resume restarts a still-playing voice at exit.
    originalResume(&collection, 0, 2);
    CHECK(voice.resumes == 1 && voice.position == 0 && voice.paused == -1);

    voice = Sound{voiceTable};
    ambient = Sound{streamTable, 1};
    ui = Sound{streamTable, 1};
    other = Sound{unrelatedTable, 1};
    ResumeSounds(&collection, nullptr, 0, 2);
    CHECK(voice.resumes == 0 && voice.position == 3 && voice.paused == 0);
    CHECK(ambient.resumes == 1 && ambient.paused == 0 && ambient.position == 3);
    CHECK(ui.resumes == 1 && other.resumes == 1);

    for (int pause : {-1, 0, 1, 2}) {
        voice = Sound{voiceTable, pause};
        ResumeSounds(&collection, nullptr, 0, 2);
        CHECK(voice.resumes == (pause > 0 ? 1 : 0));
        CHECK(voice.paused == (pause > 0 ? pause - 1 : pause) && voice.position == 3);
    }

    voice = Sound{voiceTable, 1};
    entries[0].flags |= 2; // Native nonstoppable flag still excludes this sound.
    ResumeSounds(&collection, nullptr, 0, 2);
    CHECK(voice.resumes == 0 && voice.paused == 1);

    entries[0].flags = 0x10000;
    voice = Sound{voiceTable};
    enabled = false;
    ResumeSounds(&collection, nullptr, 0, 2);
    CHECK(voice.resumes == 1 && voice.position == 0);

    enabled = true;
    voice = Sound{voiceTable};
    ResumeSounds(&collection, nullptr, 0x10000, 0x10000); // Explicit voice resume passes through.
    CHECK(voice.resumes == 1 && voice.position == 0);

    Collection empty;
    ResumeSounds(&empty, nullptr, 0, 2);

    speechStreamTable = nullptr;
    speechBufferTable = nullptr;
    std::puts(
        "PASS: exit resume regression; unpaused voice continues, paused audio resumes, other sounds and masks preserved");
}

int main(int argc, char** argv) {
    callbacks = {disco_dialogues::Event, disco_dialogues::Speech, disco_dialogues::Resume};
    taskTable[2] = reinterpret_cast<void*>(&QueryTask);
    scriptedTable[3] = reinterpret_cast<void*>(&TaskScript);
    controllerTable[3] = reinterpret_cast<void*>(&ControllerScript);
    originalResume = reinterpret_cast<ResumeFn>(&NativeResume);
    void* nativeCode = nullptr;
    if (argc == 2) {
        // verify_hd_abi.py supplies validated, position-independent getter
        // bodies from Game.exe and ResumeWithFlags from Engine.dll.
        std::array<char, 120> code{};
        std::ifstream input(argv[1], std::ios::binary);
        CHECK(input.read(code.data(), code.size()) && input.peek() == EOF);
        nativeCode = ::VirtualAlloc(nullptr, code.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        CHECK(nativeCode);
        std::memcpy(nativeCode, code.data(), code.size());
        DWORD previous;
        CHECK(::VirtualProtect(nativeCode, code.size(), PAGE_EXECUTE_READ, &previous));
        CHECK(::FlushInstructionCache(::GetCurrentProcess(), nativeCode, code.size()));
        controllerTable[3] = nativeCode;
        scriptedTable[3] = static_cast<char*>(nativeCode) + 31;
        originalResume = reinterpret_cast<ResumeFn>(static_cast<char*>(nativeCode) + 35);
    }

    TestExitResume(false);
    TestExitResume(true);

    originalEvent = reinterpret_cast<EventFn>(&Dispatch);
    originalStop = reinterpret_cast<NativeFn>(&NativeStop);
    originalPlay = reinterpret_cast<NativeFn>(&NativePlay);

    int script1 = 1, script2 = 2;
    Actor actor, other;
    Native native{nullptr, &actor}, otherNative{nullptr, &other};
    actor.Start(&script1);
    other.Start(&script2);
    CHECK(actor.Get<void*>(0x16c) != &script1);
    CHECK(hd::Method<void*(__thiscall*)(void*)>(&actor.controller, 3)(&actor.controller) == &script1);

    // Repeated instant choices and inline redirects leave voice frames untouched.
    // No waiter, queued callback or deferred branch is created.
    handler = [&](void*, unsigned, unsigned) {
        CHECK(Stop(native));
        CHECK(Stop(native));
        CHECK(Play(native));
        CHECK(actor.Get<float>(0x330) == 1.25f && actor.Get<float>(0x334) == 7.0f);
        CHECK(actor.Get<void*>(0x32c) == nullptr);
    };
    for (int i = 0; i < 10000; ++i)
        Event(&script1);
    CHECK(branchCalls == 10000 && stopCalls == 0 && playCalls == 0 && replyScope == nullptr);
    CHECK(instance == reinterpret_cast<void*>(0x1234));

    // The rest of gameplay has unmodified stop/play semantics.
    CHECK(!Stop(native));
    CHECK(!Play(native));
    CHECK(stopCalls == 1 && playCalls == 1);

    // Each invalid/unavailable speech state must pass through, never wait.
    handler = [&](void*, unsigned, unsigned) { CHECK(!Stop(native)); };
    for (int mode = 0; mode < 12; ++mode) {
        actor.Start(&script1);
        if (mode == 0)
            actor.Set<float>(0x334, 0);
        if (mode == 1)
            actor.Set<float>(0x330, 7);
        if (mode == 2)
            actor.Set<float>(0x330, -1);
        if (mode == 3)
            actor.Set<float>(0x334, std::numeric_limits<float>::infinity());
        if (mode == 4)
            actor.Set<float>(0x330, std::numeric_limits<float>::quiet_NaN());
        if (mode == 5)
            actor.Set<unsigned char>(0x24, 0);
        if (mode == 6)
            actor.Set<void*>(0x300, nullptr);
        if (mode == 7)
            actor.Set<void*>(0x32c, &script2); // Existing latent waiter.
        if (mode == 8)
            actor.Set<void*>(0x16c, nullptr);
        if (mode == 9)
            actor.controller.task = nullptr;
        if (mode == 10)
            actor.task.supportsScript = false;
        if (mode == 11)
            actor.task.script = &script2;
        const int before = stopCalls;
        Event(&script1);
        CHECK(stopCalls == before + 1);
    }

    actor.Start(&script1);
    enabled = false;
    Event(&script1);
    enabled = true;
    actor.Start(&script1);
    Event(&script1, 11, 1); // Trade/simple reply.
    actor.Start(&script1);
    Event(&script1, 6, 0); // Unload.

    actor.Start(&script1);
    other.Start(&script2);
    handler = [&](void*, unsigned, unsigned) {
        CHECK(!Stop(otherNative));
        CHECK(Stop(native));
    };
    Event(&script1);
    CHECK(other.Get<float>(0x334) == 0 && actor.Get<float>(0x334) == 7);

    // Nested lifecycle events disable preservation, then restore outer scope.
    for (unsigned cleanup : {6u, 32u, 42u}) {
        actor.Start(&script1);
        handler = [&](void* script, unsigned event, unsigned) {
            if (event != 11) {
                CHECK(!Stop(native));
                return;
            }
            CHECK(Stop(native));
            Event(script, cleanup, 0);
            CHECK(replyScope && replyScope->script == script);
            CHECK(!Play(native)); // Cleanup completed; new speech may now start.
        };
        Event(&script1);
        CHECK(replyScope == nullptr);
    }

    // Nested dialogue for a different actor does not steal the outer scope.
    actor.Start(&script1);
    other.Start(&script2);
    handler = [&](void* script, unsigned, unsigned) {
        if (script == &script2) {
            CHECK(Stop(otherNative));
            CHECK(Play(otherNative));
            return;
        }
        CHECK(Stop(native));
        Event(&script2);
        CHECK(Play(native));
    };
    Event(&script1);

    // Natural completion admits a new speech; wrong arities are never swallowed.
    actor.Start(&script1);
    handler = [&](void*, unsigned, unsigned) {
        CHECK(Stop(native));
        actor.Set<float>(0x330, 0);
        actor.Set<float>(0x334, 0);
        CHECK(!Play(native));
        CHECK(!Stop(native, 1));
        CHECK(!Play(native, 2));
    };
    Event(&script1);

    // A script error must not leave thread-local protection active afterwards.
    handler = [](void*, unsigned, unsigned) { throw std::runtime_error("script error"); };
    try {
        Event(&script1);
        CHECK(false);
    } catch (const std::runtime_error&) {
    }
    CHECK(replyScope == nullptr);
    actor.Start(&script1);
    CHECK(!Stop(native));
    std::puts(
        "PASS: real x86 hooks; HD controller/task layout; 10000 immediate replies; nested events, cleanup, replacement speech, absent voice, waiters and errors");

    // The adapter itself must contain no reply-preservation policy.
    callbacks = {};
    actor.Start(&script1);
    handler = [&](void*, unsigned, unsigned) { CHECK(!Stop(native)); };
    const int beforePassthrough = stopCalls;
    Event(&script1);
    CHECK(stopCalls == beforePassthrough + 1 && actor.Get<float>(0x334) == 0);

    int token = 0;
    callbackData = &token;
    callbacks.speech = [](const OynonSpeechCall* call, void* data) -> BOOL {
        ++*static_cast<int*>(data);
        CHECK(call->count == 42);
        return TRUE;
    };
    CHECK(Stop(native, 42) && token == 1 && stopCalls == beforePassthrough + 1);

    callbacks.event = [](const OynonScriptEventCall* call, void* data) -> int {
        ++*static_cast<int*>(data);
        CHECK(call->event == 6 && call->count == 0);
        return 123;
    };
    CHECK(SendEvent(&script1, nullptr, 6, 0, arguments) == 123 && token == 2);
    std::puts("PASS: generic passthrough and unrelated client event/speech policies with userData");

    if (nativeCode) {
        std::puts("PASS: executed native GetScript bodies extracted from installed HD Game.exe");
        CHECK(::VirtualFree(nativeCode, 0, MEM_RELEASE));
    }
}
