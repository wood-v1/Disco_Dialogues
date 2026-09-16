#pragma once

#include <windows.h>
#include <cwchar>

namespace disco_dialogues {
struct ClientSize {
    int width = 0;
    int height = 0;
    int candidates = 0;
};

inline BOOL CALLBACK FindGameWindow(HWND window, LPARAM data) {
    DWORD process = 0;
    ::GetWindowThreadProcessId(window, &process);
    if (process != ::GetCurrentProcessId() || !::IsWindowVisible(window) || ::IsIconic(window) ||
        ::GetWindow(window, GW_OWNER))
        return TRUE;

    // Pathologic HD's main window class is independent of the localized title.
    // Debug consoles and renderer proxy windows must not affect layout selection.
    wchar_t className[256]{};
    if (!::GetClassNameW(window, className, 256) || std::wcscmp(className, L"PlagueCityClass") != 0)
        return TRUE;

    RECT rect{};
    if (!::GetClientRect(window, &rect) || rect.right <= 0 || rect.bottom <= 0)
        return TRUE;

    auto& size = *reinterpret_cast<ClientSize*>(data);
    ++size.candidates;
    size.width = rect.right;
    size.height = rect.bottom;
    return TRUE;
}
}
