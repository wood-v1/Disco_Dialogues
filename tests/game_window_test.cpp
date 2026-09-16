#include "../src/game_window.h"
#include "../src/layout_selection.h"
#include <cstdio>

namespace {
HWND Create(const wchar_t* className, const wchar_t* title, int width, int height, HWND owner = nullptr) {
    return ::CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, className, title, WS_POPUP | WS_VISIBLE,
        -32000, -32000, width, height, owner, nullptr, ::GetModuleHandleW(nullptr), nullptr);
}

disco_dialogues::ClientSize Measure() {
    disco_dialogues::ClientSize size;
    ::EnumWindows(disco_dialogues::FindGameWindow, reinterpret_cast<LPARAM>(&size));
    return size;
}
}

int main() {
    ::SetProcessDPIAware();
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = ::DefWindowProcW;
    windowClass.hInstance = ::GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"PlagueCityClass";
    if (!::RegisterClassW(&windowClass))
        return 1;

    // Same process, visible, unowned; even a matching title and larger size must not qualify.
    const HWND console = Create(L"STATIC", L"OynonTools Debug Console", 960, 480);
    const HWND auxiliary = Create(L"STATIC", L"PlagueCityClass", 2560, 1440);
    if (!console || !auxiliary || Measure().candidates != 0)
        return 2;

    const HWND game = Create(L"PlagueCityClass", L"Localized game title", 1920, 1080);
    const HWND owned = Create(L"PlagueCityClass", L"Owned helper", 800, 600, game);
    if (!game || !owned)
        return 3;
    auto size = Measure();
    if (size.candidates != 1 || size.width != 1920 || size.height != 1080 ||
        !disco_dialogues::SelectLayout("dialog.xml", size.width, size.height))
        return 4;

    ::ShowWindow(game, SW_HIDE);
    if (Measure().candidates != 0)
        return 5;
    ::ShowWindow(game, SW_SHOWNOACTIVATE);
    ::SetWindowPos(game, nullptr, 0, 0, 1600, 900, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    size = Measure();
    if (size.candidates != 1 || size.width != 1600 || size.height != 900)
        return 6;

    ::DestroyWindow(owned);
    ::DestroyWindow(game);
    if (Measure().candidates != 0)
        return 7;
    ::DestroyWindow(auxiliary);
    ::DestroyWindow(console);
    ::UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
    std::puts("Game window selection with debug console, auxiliary windows and resizing: PASS");
    return 0;
}
