#include "dialog_policy.h"
#include "OynonRuntimeApi.h"
#include "camera_framing.h"
#include "camera_transition.h"

namespace disco_dialogues {
namespace {
bool StandardDialogTransition() {
    unsigned op = 0, count = 0;
    if (!OynonGetActiveInstruction(&op, &count) || count > 1000000)
        return false;

    char name[128]{};
    bool readable = true;
    const bool matched = IsDialogCameraBlock(op, count, [&](unsigned index) -> const char* {
        if (!OynonGetActiveNativeName(index, name, sizeof(name))) {
            readable = false;
            return nullptr;
        }
        return *name ? name : nullptr;
    });
    return readable && matched;
}

BOOL __stdcall Transit(const OynonCameraTransitCall* call, void*) {
    if ((call->count != 2 && call->count != 3) || !StandardDialogTransition())
        return OynonProceedCameraTransit(call, nullptr);

    const float fraction = ResolveDialogLayoutFraction();
    if (fraction <= 0.0f)
        return OynonProceedCameraTransit(call, nullptr);

    OynonCameraTransitState state{};
    if (!OynonReadCameraTransit(call, &state))
        return OynonProceedCameraTransit(call, nullptr);

    Vector3 direction{state.direction.x, state.direction.y, state.direction.z};
    if (!FrameOnLeft(direction, {0.0f, 1.0f, 0.0f}, state.importFov, fraction))
        return OynonProceedCameraTransit(call, nullptr);

    const OynonVector3 framed{direction.x, direction.y, direction.z};
    return OynonProceedCameraTransit(call, &framed);
}
}

bool InstallDialogCamera() {
    return OynonInstallCameraTransitHook(Transit, nullptr) != FALSE;
}
}
