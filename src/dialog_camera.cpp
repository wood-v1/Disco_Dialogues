#include "dialog_camera.h"
#include "camera_framing.h"
#include "camera_transition.h"
#include "hd_hooks.h"
#include "camera_abi.h"

namespace disco_dialogues {
namespace {
using InstructionFn = bool(__thiscall*)(void*, void*, float);
using TransitFn = bool(__thiscall*)(void*, void*&, void**, unsigned, void*);
using NameFn = const char*(__thiscall*)(void*, unsigned);
InstructionFn originalInstruction = nullptr;
TransitFn originalTransit = nullptr;
NameFn globalName = nullptr;
void** nativeInstructionTable = nullptr;
thread_local void* executingData = nullptr;

bool __fastcall Instruction(void* self, void*, void* data, float dt) {
    // Native script calls can nest. This scope identifies the real calling BIN
    // instruction without polling, actor-name guesses, or persistent pointers.
    struct Scope {
        void* previous;
        ~Scope() { executingData = previous; }
    } scope{executingData};
    executingData = data;
    return originalInstruction(self, data, dt);
}

const char* NativeName(void* data, void** code, unsigned index) {
    auto instruction = static_cast<char*>(code[index]);
    if (*reinterpret_cast<void***>(instruction) != nativeInstructionTable) return nullptr;
    return globalName(data, *reinterpret_cast<unsigned*>(instruction + 4));
}

bool StandardDialogTransition() {
    // Read only verified HD interpreter structures. An unknown/malformed script
    // fails closed; no game object or script is changed during classification.
    __try {
        if (!executingData) return false;
        auto data = static_cast<char*>(executingData);
        auto script = *reinterpret_cast<char**>(data);
        unsigned count = *reinterpret_cast<unsigned*>(script + 0x30);
        unsigned op = *reinterpret_cast<unsigned*>(data + 0x2c);
        if (count > 1000000) return false;
        auto code = *reinterpret_cast<void***>(script + 0x34);
        return IsDialogCameraBlock(op, count, [=](unsigned i) { return NativeName(data, code, i); });
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool __fastcall Transit(void* self, void*, void*& instance, void** args, unsigned count, void* result) {
    if ((count != 2 && count != 3) || !StandardDialogTransition())
        return originalTransit(self, instance, args, count, result);
    const float fraction = ResolveDialogLayoutFraction();
    if (fraction <= 0.0f) return originalTransit(self, instance, args, count, result);
    auto context = *reinterpret_cast<void**>(static_cast<char*>(self) + 4);
    // Same virtual calls as HD _ScriptCameraTransit and CCameraBase constructor.
    void* world = hd::Method<void*(__thiscall*)(void*)>(context, 8)(context);
    void* camera = world ? hd::Method<void*(__thiscall*)(void*)>(world, 34)(world) : nullptr;
    if (!camera) return originalTransit(self, instance, args, count, result);
    const float fov = hd::CameraImportFov(camera);
    Vector3 original{};
    using GetVector = bool(__thiscall*)(void*, Vector3&);
    using SetVector = bool(__thiscall*)(void*, const Vector3&);
    if (!hd::Method<GetVector>(args[1], 11)(args[1], original))
        return originalTransit(self, instance, args, count, result);
    Vector3 framed = original;
    if (!FrameOnLeft(framed, {0.0f, 1.0f, 0.0f}, fov, fraction))
        return originalTransit(self, instance, args, count, result);
    const auto set = hd::Method<SetVector>(args[1], 5);
    if (!set(args[1], framed)) return originalTransit(self, instance, args, count, result);
    struct Restore {
        void* var; SetVector set; Vector3 value;
        ~Restore() { set(var, value); }
    } restore{args[1], set, original};
    // The native constructor now receives the final target rotation before its
    // first Update. Position, FOV, speeds and return-to-gameplay remain native.
    return originalTransit(self, instance, args, count, result);
}
}

bool InstallDialogCamera() {
    auto game = ::GetModuleHandleW(nullptr);
    auto engine = ::GetModuleHandleW(L"Engine.dll");
    if (!hd::Module(game, 0x5698d115, 0x4c4000) || !hd::Module(engine, 0x561b8394, 0x26e000)) return false;
    auto g = reinterpret_cast<BYTE*>(game);
    auto e = reinterpret_cast<BYTE*>(engine);
    if (!hd::Bytes(g, 0x24e030, "\xff\x74\x24\x10\x8b\x49\x04", 7) ||
        !hd::Bytes(g, 0x153706, "\x8b\x01\x8b\x40\x28\xff\xd0", 7) ||
        !hd::Bytes(g, 0x2560c5, "\x8b\x01\x8b\x40\x2c\xff\xd0", 7) ||
        !hd::Bytes(g, 0x2560e3, "\x8b\x06\x8b\xce\x8b\x40\x20\xff\xd0", 9) ||
        !hd::Bytes(g, 0x2560f2, "\x8b\x92\x88\x00\x00\x00\xff\xd2", 8) ||
        !hd::Bytes(e, 0x131c82, "\x8b\x4f\x2c", 3) ||
        !hd::Bytes(e, 0x131c8e, "\x8b\x07\x3b\x48\x30", 5) ||
        !hd::Bytes(e, 0x131cf9, "\x8b\x40\x34", 3) ||
        !hd::Bytes(e, 0x13a212, "\xff\x75\x04\x8b\xcb", 5) ||
        !hd::Bytes(e, 0x131a80, "\x8b\x41\x04\x8b\x10", 5) ||
        !hd::Bytes(e, 0x126ac0, "\x8b\x44\x24\x04\xf3\x0f\x7e\x00", 8)) return false;
    hd::Slot slots[] = {
        {reinterpret_cast<void**>(e + 0x205684), e + 0x13a1e0, reinterpret_cast<void*>(&Instruction)},
        {reinterpret_cast<void**>(g + 0x37ba44), g + 0x23d21, reinterpret_cast<void*>(&Transit)},
    };
    for (const auto& slot : slots) if (*slot.address != slot.expected) return false;
    originalInstruction = reinterpret_cast<InstructionFn>(slots[0].expected);
    originalTransit = reinterpret_cast<TransitFn>(slots[1].expected);
    globalName = reinterpret_cast<NameFn>(e + 0x131a80);
    nativeInstructionTable = reinterpret_cast<void**>(e + 0x205680);
    if (!hd::Replace(slots[0])) return false;
    if (!hd::Replace(slots[1])) { hd::Replace(slots[0], true); return false; }
    return true;
}
}
