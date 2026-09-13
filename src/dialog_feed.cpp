#include "dialog_feed.h"
#include "hd_hooks.h"

namespace disco_dialogues {
namespace {
using ExecuteFn = bool(__thiscall*)(void*, const char*, void**, unsigned, void*);
ExecuteFn originalExecute = nullptr;

// HD has no script API for a subrectangle of an owner-drawn window. Keep native
// text wrapping/localisation and limit just this draw call to one feed viewport.
// The full window rectangle is restored before input dispatch or any other draw.
bool __fastcall Execute(void* context, void*, const char* name, void** args, unsigned count, void* result) {
    if (!name || std::strcmp(name, "DiscoDialoguesPrint") != 0)
        return originalExecute(context, name, args, count, result);
    if (count != 11) return false;
    int top = 0, height = 0;
    if (!hd::GetInt(args[9], top) || !hd::GetInt(args[10], height)) return false;
    auto window = static_cast<char*>(context) - 0x28;
    auto& originY = *reinterpret_cast<int*>(window + 0xc8);
    auto& windowHeight = *reinterpret_cast<int*>(window + 0xd0);
    auto& clipping = *reinterpret_cast<bool*>(window + 0xe3);
    if (top < 0 || height <= 0 || top > windowHeight || height > windowHeight - top) return false;
    struct Restore {
        int& y; int& h; bool& clip; int oldY; int oldH; bool oldClip;
        ~Restore() { y = oldY; h = oldH; clip = oldClip; }
    } restore{originY, windowHeight, clipping, originY, windowHeight, clipping};
    originY += top;
    windowHeight = height;
    clipping = true;
    return originalExecute(context, "PrintInWidth", args, 9, result);
}
}
bool InstallDialogFeed() {
    const auto module = ::GetModuleHandleW(L"UI.dll");
    if (!hd::Module(module, 0x5654c15f, 0xcf000)) return false;
    const auto ui = reinterpret_cast<BYTE*>(module);
    // IUIContext adjustment, GetWindowSize fields and clip rectangle in HD.
    if (!hd::Bytes(ui, 0x2e7d3, "\x8d\x4d\xd8", 3) ||
        !hd::Bytes(ui, 0x327a5, "\xff\xb6\xcc\x00\x00\x00", 6) ||
        !hd::Bytes(ui, 0x327c0, "\xff\xb6\xd0\x00\x00\x00", 6) ||
        !hd::Bytes(ui, 0x3536d, "\x8d\x54\x24\x10\x52\x8b\x01\x8b\x40\x40", 10) ||
        !hd::Bytes(ui, 0x35405, "\x80\xbf\xe3\x00\x00\x00\x00", 7) ||
        !hd::Bytes(ui, 0x3540e, "\x8b\x8f\xc8\x00\x00\x00\x8b\x87\xd0\x00\x00\x00", 12)) return false;
    hd::Slot slot{reinterpret_cast<void**>(ui + 0xa4df4 + 3*4), ui + 0x4390, reinterpret_cast<void*>(&Execute)};
    if (*slot.address != slot.expected) return false;
    originalExecute = reinterpret_cast<ExecuteFn>(slot.expected);
    return hd::Replace(slot);
}
}
