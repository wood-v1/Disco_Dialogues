// Real adapter plus mod callback: exercise scoped engine edits and a second
// client's unrelated command, without installing hooks in a game process.
#include "ui_execute_hook.cpp"
#include "../src/dialog_feed.cpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::abort(); } } while (0)
namespace {
struct Var { void** table; int value; bool valid = true; };
bool __fastcall GetInt(Var* var, void*, int& value) { value = var->value; return var->valid; }
std::array<char, 0x100> window{};
int& Y() { return *reinterpret_cast<int*>(window.data() + 0xc8); }
int& Height() { return *reinterpret_cast<int*>(window.data() + 0xd0); }
bool& Clip() { return *reinterpret_cast<bool*>(window.data() + 0xe3); }
unsigned calls = 0;
bool expectViewport = false, failNative = false;
const char* expectedName = nullptr;
unsigned expectedCount = 0;
void** expectedArgs = nullptr;
bool __fastcall Native(void* context, void*, const char* name, void** args, unsigned count, void* result) {
    CHECK(context == window.data() + 0x28 && args == expectedArgs && result == window.data());
    CHECK((!name && !expectedName) || (name && expectedName && std::strcmp(name, expectedName) == 0));
    CHECK(count == expectedCount);
    CHECK(Y() == (expectViewport ? 27 : 20));
    CHECK(Height() == (expectViewport ? 30 : 100));
    CHECK(Clip() == expectViewport);
    ++calls;
    if (failNative) throw std::runtime_error("draw error");
    return false;
}
}
int main() {
    using namespace oynon::runtime;
    originalExecute = reinterpret_cast<ExecuteFn>(&Native);
    callback = disco_dialogues::Execute;
    Y() = 20; Height() = 100; Clip() = false;
    void* table[17]{};
    table[16] = reinterpret_cast<void*>(&GetInt);
    Var top{table, 7}, height{table, 30};
    void* args[11]{};
    args[9] = &top; args[10] = &height;
    expectedArgs = args;
    auto invoke = [&](const char* name, unsigned count) {
        return Execute(window.data() + 0x28, nullptr, name, args, count, window.data());
    };
    expectedName = "Ordinary"; expectedCount = 3;
    CHECK(!invoke("Ordinary", 3));
    expectedName = nullptr;
    CHECK(!invoke(nullptr, 3));
    CHECK(calls == 2);
    CHECK(!invoke("DiscoDialoguesPrint", 10));
    top.valid = false;
    CHECK(!invoke("DiscoDialoguesPrint", 11));
    top.valid = true;
    top.value = -1;
    CHECK(!invoke("DiscoDialoguesPrint", 11));
    top.value = 90;
    CHECK(!invoke("DiscoDialoguesPrint", 11));
    CHECK(calls == 2);
    top.value = 7;
    expectedName = "PrintInWidth"; expectedCount = 9; expectViewport = true;
    CHECK(!invoke("DiscoDialoguesPrint", 11));
    CHECK(calls == 3 && Y() == 20 && Height() == 100 && !Clip());
    failNative = true;
    try { invoke("DiscoDialoguesPrint", 11); CHECK(false); } catch (const std::runtime_error&) {}
    CHECK(Y() == 20 && Height() == 100 && !Clip());
    failNative = false;

    // Another mod chooses an unrelated name/count and uses the same viewport API.
    int token = 0;
    callbackData = &token;
    callback = [](const OynonUIExecuteCall* call, void* data) -> BOOL {
        ++*static_cast<int*>(data);
        const OynonVerticalViewport viewport{7, 30};
        return OynonProceedUIExecute(call, "OtherNative", 2, &viewport);
    };
    expectedName = "OtherNative"; expectedCount = 2;
    CHECK(!invoke("OtherModCommand", 4));
    CHECK(token == 1 && Y() == 20 && Height() == 100 && !Clip());
    std::puts("PASS: UI forwarding, mod command validation, scoped viewport/error restoration, unrelated client callback");
}
