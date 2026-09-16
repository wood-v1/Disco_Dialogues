#include "OynonToolsApi.h"
#include "dialog_policy.h"
#include "game_window.h"
#include "layout_selection.h"
#include <cstring>
#include <cstdio>
#include <string>
#include <atomic>

namespace {
std::wstring iniPath;
std::wstring uiDirectory;
bool debugEnabled = false;
bool cameraEnabled = true;
bool preserveNpcSpeech = true;
std::atomic<bool> runtimeReady{false};

void Trace(const char* text) {
    if (debugEnabled) {
        ::OutputDebugStringA("DiscoDialogues: ");
        ::OutputDebugStringA(text);
        ::OutputDebugStringA("\n");
    }
}

using disco_dialogues::ClientSize;
using disco_dialogues::FindGameWindow;

const char* CurrentLayout(ClientSize& size) {
    if (!runtimeReady.load(std::memory_order_acquire))
        return nullptr;

    // Match physical client pixels even when the calling thread is DPI virtualized.
    using SetDpi = HANDLE(WINAPI*)(HANDLE);
    const auto setDpi = reinterpret_cast<SetDpi>(
        ::GetProcAddress(::GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext"));
    const HANDLE previous = setDpi ? setDpi(reinterpret_cast<HANDLE>(-4)) : nullptr;
    ::EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&size));
    if (setDpi && previous)
        setDpi(previous);

    const char* replacement = nullptr;
    if (size.candidates == 1 && ::GetPrivateProfileIntW(L"General", L"Enabled", 1, iniPath.c_str())) {
        replacement = disco_dialogues::SelectLayout("dialog.xml", size.width, size.height);
    }

    if (replacement) {
        const std::string name(replacement);
        const std::wstring path = uiDirectory + std::wstring(name.begin(), name.end());
        const DWORD attributes = ::GetFileAttributesW(path.c_str());
        const std::wstring accents = uiDirectory + L"..\\Textures\\ui\\disco_dialogues_accents.tga";
        const DWORD accentAttributes = ::GetFileAttributesW(accents.c_str());

        bool scriptsPresent = true;
        for (const auto component : {L"feed", L"panel", L"photo", L"title"}) {
            const std::wstring script = uiDirectory + L"..\\Scripts\\disco_dialogues_" + component + L".bin";
            const DWORD scriptAttributes = ::GetFileAttributesW(script.c_str());
            if (scriptAttributes == INVALID_FILE_ATTRIBUTES || (scriptAttributes & FILE_ATTRIBUTE_DIRECTORY))
                scriptsPresent = false;
        }

        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) ||
            !scriptsPresent || accentAttributes == INVALID_FILE_ATTRIBUTES ||
            (accentAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            Trace("layout, accent texture or UI script missing; using vanilla dialog");
            replacement = nullptr;
        }
    }

    return replacement;
}

void __stdcall Prepare(const char* xml, void*) {
    if (!xml || std::strcmp(xml, "dialog.xml") != 0)
        return;

    ClientSize size;
    const char* replacement = CurrentLayout(size);

    // Re-evaluate every opening, including disable/resolution changes: clear stale redirects.
    if (!OynonUISetWindowRedirect(xml, replacement)) {
        Trace("redirect configuration failed");
        replacement = nullptr;
    }

    char line[256]{};
    std::snprintf(line, sizeof(line), "prepare %s client=%dx%d windows=%d -> %s", xml, size.width,
        size.height, size.candidates, replacement ? replacement : xml);
    Trace(line);
}

void __stdcall Created(const char* original, const char* resolved, BOOL succeeded, DWORD, void*) {
    if (!original || std::strcmp(original, "dialog.xml") != 0)
        return;

    char line[256]{};
    std::snprintf(line, sizeof(line), "created %s -> %s success=%d", original, resolved ? resolved : "(null)",
        succeeded);
    Trace(line);
}

DWORD WINAPI Initialize(void* parameter) {
    wchar_t path[32768]{};
    DWORD length = ::GetModuleFileNameW(static_cast<HMODULE>(parameter), path, 32768);
    if (!length || length >= 32768)
        return 1;

    const std::wstring module(path, length);
    iniPath = module.substr(0, module.find_last_of(L"\\/") + 1) + L"DiscoDialogues.ini";

    length = ::GetModuleFileNameW(nullptr, path, 32768);
    if (!length || length >= 32768)
        return 1;

    const std::wstring exe(path, length);
    uiDirectory = exe.substr(0, exe.find_last_of(L"\\/") + 1) + L"..\\..\\data\\UI\\";

    debugEnabled = ::GetPrivateProfileIntW(L"Debug", L"Enabled", 0, iniPath.c_str()) != 0;
    cameraEnabled = ::GetPrivateProfileIntW(L"Camera", L"FrameNPCOnLeft", 1, iniPath.c_str()) != 0;
    preserveNpcSpeech = ::GetPrivateProfileIntW(L"Audio", L"PreserveNpcSpeech", 1, iniPath.c_str()) != 0;
    if (::GetPrivateProfileIntW(L"Debug", L"SpeechTrace", 0, iniPath.c_str()))
        disco_dialogues::InitializeDialogSpeechTrace((iniPath + L".speech.log").c_str());
    if (!::GetPrivateProfileIntW(L"General", L"Enabled", 1, iniPath.c_str()))
        return 0;

    // OynonTools has no listener removal API. Keep this DLL loaded for callback lifetime.
    HMODULE pinned = nullptr;
    if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, module.c_str(), &pinned))
        return 1;

    if (!disco_dialogues::InstallDialogFeed()) {
        ::OutputDebugStringA("DiscoDialogues: unsupported or occupied UI ABI; leaving vanilla UI enabled\n");
        return 1;
    }

    if (cameraEnabled && !disco_dialogues::InstallDialogCamera()) {
        cameraEnabled = false;
        ::OutputDebugStringA(
            "DiscoDialogues: camera hooks unavailable for this engine build; layout remains enabled\n");
    }

    if (preserveNpcSpeech && !disco_dialogues::InstallDialogSpeech()) {
        preserveNpcSpeech = false;
        ::OutputDebugStringA("DiscoDialogues: speech hooks unavailable; using vanilla speech interruption\n");
    }

    if (!OynonRegisterUIWindowPrepareCallback(Prepare, nullptr) ||
        !OynonRegisterUIWindowCreatedCallback(Created, nullptr))
        return 1;

    if (!OynonInitializeHooksWhenReady(OYNON_HOOK_UI_WINDOW_PREPARE)) {
        Trace("UI hook initialization failed");
        return 1;
    }

    Trace("Disco Dialogues 1.0.0 initialized");
    runtimeReady.store(true, std::memory_order_release);

    for (;;) {
        // Shared hook resilience; no inventory, input, effects or gameplay polling.
        OynonUIPoll();
        ::Sleep(250);
    }
}
}

namespace disco_dialogues {
bool ShouldPreserveDialogSpeech() {
    if (!runtimeReady.load(std::memory_order_acquire) || !preserveNpcSpeech)
        return false;

    ClientSize size;
    return CurrentLayout(size) != nullptr;
}

float ResolveDialogLayoutFraction() {
    if (!cameraEnabled)
        return 0.0f;

    ClientSize size;
    if (!CurrentLayout(size))
        return 0.0f;

    return LayoutLeftFraction(size.width, size.height);
}
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        ::DisableThreadLibraryCalls(module);
        const HANDLE thread = ::CreateThread(nullptr, 0, Initialize, module, 0, nullptr);
        if (thread)
            ::CloseHandle(thread);
    }
    return TRUE;
}
