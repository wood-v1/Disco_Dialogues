#pragma once
#include <windows.h>
#include <cstring>

namespace disco_dialogues::hd {
inline bool Module(HMODULE module, DWORD timestamp, DWORD size) {
    if (!module) return false;
    const auto bytes = reinterpret_cast<const BYTE*>(module);
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(bytes + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
        nt->FileHeader.TimeDateStamp == timestamp && nt->OptionalHeader.SizeOfImage == size;
}
inline bool Bytes(const BYTE* image, DWORD rva, const char* bytes, size_t size) {
    return std::memcmp(image + rva, bytes, size) == 0;
}
struct Slot { void** address; void* expected; void* replacement; };
inline bool Replace(const Slot& slot, bool restore = false) {
    DWORD protection = 0;
    if (!::VirtualProtect(slot.address, sizeof(void*), PAGE_READWRITE, &protection)) return false;
    void* expected = restore ? slot.replacement : slot.expected;
    void* replacement = restore ? slot.expected : slot.replacement;
    const bool changed = ::InterlockedCompareExchangePointer(slot.address, replacement, expected) == expected;
    DWORD ignored = 0;
    ::VirtualProtect(slot.address, sizeof(void*), protection, &ignored);
    return changed;
}
template<typename Fn> Fn Method(void* object, unsigned slot) {
    return reinterpret_cast<Fn>((*static_cast<void***>(object))[slot]);
}
inline bool GetInt(void* var, int& value) {
    return Method<bool(__thiscall*)(void*, int&)>(var, 16)(var, value);
}
}
