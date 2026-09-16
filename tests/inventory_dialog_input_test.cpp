#include <atomic>
#include <string>
#include <array>
#include <cstdlib>
#include "OynonToolsApi.h"
#include "dialog_input_state.h"
#define private public
#include "input_bridge.h"
#undef private
#include "diagnostics.h"
#include "runtime_state.h"

// Execute the real Inventory Overhaul input bridge with deterministic transports
// and the actual shared modal gate. No game process, effects or real keyboard.
static DialogInputState modal;
static std::array<bool, 256> keys{};
static int effects = 0, commands = 0;

static bool KeyDown(unsigned key) {
    return keys[key];
}

static BOOL Gate() {
    return modal.Blocks(KeyDown);
}

static DWORD Generation() {
    return modal.Generation();
}

static BOOL Overlay() {
    return OYNON_INVENTORY_OVERLAY_NONE;
}

static BOOL Apply(const char*) {
    ++effects;
    return TRUE;
}

static BOOL Command(const char*) {
    ++commands;
    return TRUE;
}

static BOOL RegisterConsole(OynonConsoleCommandFilter, void*) {
    return TRUE;
}

static BOOL RegisterKeyboard(OynonKeyboardCallback, void*) {
    return TRUE;
}

#define OynonUIDialogBlocksItemHotkeys Gate
#define OynonUIDialogInputGeneration Generation
#define OynonUIInventoryGetOverlayKind Overlay
#define OynonApplyObservedPlayerEffect Apply
#define OynonExecCommand Command
#define OynonRegisterConsoleCommandFilter RegisterConsole
#define OynonRegisterKeyboardCallback RegisterKeyboard
#include "input_bridge.cpp"

namespace inventory_overhaul {
Diagnostics::Diagnostics(bool, const std::string&) {
}

void Diagnostics::Log(const char*) const {
}

DWORD ReadHandCombatKey(const std::string&) {
    return 'Q';
}
}

static void Check(bool condition) {
    if (!condition)
        std::abort();
}

int main() {
    using namespace inventory_overhaul;
    RuntimeState state;
    state.playerBranch = 0;
    Diagnostics diagnostics(false, "");
    InputBridge input(state, diagnostics, "");
    input.RefreshHandCombatKey(false);

    int station = 0, unrelated = 0;
    modal.Open(&station);

    // Same modal station while character info is visible; no separate state.
    for (unsigned key :
        std::array<unsigned, 12>{'1', '2', '3', '4', '5', 'Q', 'E', 0x61, 0x62, 0x63, 0x64, 27}) {
        input.OnKeyboardInput(key, TRUE);
        input.OnKeyboardInput(key, FALSE);
    }
    Check(effects == 0 && commands == 0);

    state.pendingQuickslot = 2;
    input.RetryPendingQuickslot();
    Check(state.pendingQuickslot == 0 && effects == 0);

    input.OnConsoleCommand("handcombat");
    Check(commands == 0);

    modal.Close(&unrelated, KeyDown);
    Check(Gate());

    // Dialogue handles Leave first; inventory only observes its press afterwards.
    keys['1'] = true;
    modal.Close(&station, KeyDown);
    input.OnKeyboardInput('1', TRUE);
    input.RetryPendingQuickslot();
    Check(effects == 0);

    keys['1'] = false;
    input.OnKeyboardInput('1', FALSE);
    input.OnKeyboardInput('1', TRUE);
    Check(effects == 1);

    input.lastQuickslotRequestTick_ = 0;
    input.OnKeyboardInput(VK_NUMPAD2, TRUE);
    Check(effects == 2);

    input.OnKeyboardInput('Q', TRUE);
    Check(commands == 1);

    // Short/failed dialog lifetime between inventory polls still drains a queue.
    state.pendingQuickslot = 1;
    modal.Open(&station);
    modal.Close(&station, KeyDown);
    input.RetryPendingQuickslot();
    Check(state.pendingQuickslot == 0 && effects == 2);
    Check(!Gate());

    input.lastQuickslotRequestTick_ = 0;
    input.OnKeyboardInput('1', TRUE);
    Check(effects == 3); // First fresh press after close is not swallowed.

    // Nested stations cannot release the gate while another dialog is alive.
    modal.Open(&station);
    modal.Open(&unrelated);
    modal.Close(&station, KeyDown);
    Check(Gate());

    modal.Close(&unrelated, KeyDown);
    Gate();
    Check(!Gate());
    return 0;
}
