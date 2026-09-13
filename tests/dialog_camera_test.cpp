#include "camera_hook.cpp"
#include "../src/dialog_camera.cpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <cmath>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::abort(); } } while (0)
namespace disco_dialogues {
float fraction = 0.65f;
float ResolveDialogLayoutFraction() { return fraction; }
}
namespace {
struct Var { void** table; OynonVector3 direction{0, 0, 1}; unsigned reads = 0, writes = 0; bool writable = true; };
bool __fastcall Read(Var* var, void*, OynonVector3& value) { ++var->reads; value = var->direction; return true; }
bool __fastcall Write(Var* var, void*, const OynonVector3& value) {
    ++var->writes;
    if (!var->writable) return false;
    var->direction = value; return true;
}
struct Object { void** table; void* next; };
void* __fastcall Next(Object* object, void*) { return object->next; }
float __fastcall Fov(void*, void*) { return 1.0f; }
std::array<const char*, 60> names{};
const char* __fastcall Name(void*, void*, unsigned index) { return names[index]; }
struct Op { void** table; unsigned index; };
bool shifted = false, failNative = false;
void* expectedInstance = nullptr;
void** expectedArgs = nullptr;
bool __fastcall Native(void*, void*, void*& instance, void** args, unsigned count, void* result) {
    CHECK(&instance == &expectedInstance && args == expectedArgs && count >= 2 && result == args);
    const auto var = static_cast<Var*>(args[1]);
    CHECK(shifted ? var->direction.x > 0 : var->direction.x == 0);
    if (failNative) throw std::runtime_error("transit error");
    return false;
}
bool __fastcall ThrowInstruction(void*, void*, void* current, float) {
    CHECK(oynon::runtime::executingData == current);
    throw std::runtime_error("instruction error");
}
}
int main() {
    using namespace oynon::runtime;
    void* varTable[12]{}; varTable[11] = reinterpret_cast<void*>(&Read); varTable[5] = reinterpret_cast<void*>(&Write);
    Var direction{varTable};
    void* cameraTable[11]{}; cameraTable[10] = reinterpret_cast<void*>(&Fov);
    Object camera{cameraTable, nullptr};
    void* worldTable[35]{}; worldTable[34] = reinterpret_cast<void*>(&Next);
    Object world{worldTable, &camera};
    void* contextTable[9]{}; contextTable[8] = reinterpret_cast<void*>(&Next);
    Object context{contextTable, &world}, self{nullptr, &context};
    void* args[]{nullptr, &direction}; expectedArgs = args;
    std::array<char, 0x38> script{};
    std::array<char, 0x30> data{};
    void* nativeTable[1]{}; nativeInstructionTable = nativeTable;
    std::array<Op, 60> ops{}; std::array<void*, 60> code{};
    for (unsigned i = 0; i < ops.size(); ++i) { ops[i] = {nativeTable, i}; code[i] = &ops[i]; }
    *reinterpret_cast<unsigned*>(script.data() + 0x30) = 60;
    *reinterpret_cast<void***>(script.data() + 0x34) = code.data();
    *reinterpret_cast<char**>(data.data()) = script.data();
    *reinterpret_cast<unsigned*>(data.data() + 0x2c) = 20;
    executingData = data.data();
    globalName = reinterpret_cast<NameFn>(&Name);
    originalTransit = reinterpret_cast<TransitFn>(&Native);
    callback = disco_dialogues::Transit;
    names[11] = "IsOverrideActive"; names[17] = "StopWorld"; names[20] = "CameraTransit";
    names[24] = "Rotate"; names[33] = "HasAnimationTrack"; names[37] = "LookAsyncCamera";
    names[39] = "CameraWaitForPlayFinish"; names[41] = "ResumeWorld";
    auto invoke = [&] { return Transit(&self, nullptr, expectedInstance, args, 2, args); };
    shifted = true;
    CHECK(!invoke());
    CHECK(direction.direction.x == 0 && direction.direction.z == 1 && direction.reads == 1 && direction.writes == 2);
    failNative = true;
    try { invoke(); CHECK(false); } catch (const std::runtime_error&) {}
    CHECK(direction.direction.x == 0 && direction.direction.z == 1);
    failNative = false; shifted = false;
    const auto reads = direction.reads;
    disco_dialogues::fraction = 0;
    CHECK(!invoke() && direction.reads == reads);
    disco_dialogues::fraction = 0.65f;
    names[17] = "DifferentScript";
    CHECK(!invoke() && direction.reads == reads);
    names[17] = "StopWorld";
    code[21] = nullptr; // Corrupt instruction fails closed, even if later names match.
    CHECK(!invoke() && direction.reads == reads);
    code[21] = &ops[21];
    direction.writable = false;
    CHECK(!invoke());
    direction.writable = true;

    // Another client adjusts an unrelated transition without a dialogue pattern.
    names.fill(nullptr); executingData = nullptr;
    int token = 0; callbackData = &token;
    callback = [](const OynonCameraTransitCall* call, void* userData) -> BOOL {
        ++*static_cast<int*>(userData);
        const OynonVector3 otherDirection{1, 0, 0};
        return OynonProceedCameraTransit(call, &otherDirection);
    };
    shifted = true;
    CHECK(!invoke());
    CHECK(token == 1 && direction.direction.x == 0 && direction.direction.z == 1);

    // Interpreter scopes survive nested native calls and exception unwinding.
    originalInstruction = reinterpret_cast<InstructionFn>(&ThrowInstruction);
    executingData = script.data();
    try { Instruction(nullptr, nullptr, data.data(), 0); CHECK(false); } catch (const std::runtime_error&) {}
    CHECK(executingData == script.data());
    std::puts("PASS: camera policy, ABI forwarding, fallback, scoped vector/interpreter restoration, unrelated client callback");
}
