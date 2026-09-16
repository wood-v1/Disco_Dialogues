#pragma once

#include <cstring>

namespace disco_dialogues {
// The HD SetDialogCamera helper has this native-call sequence. Trade transitions
// omit head tracking; cutscenes do not have this guarded StopWorld sequence.
// Unknown/custom helpers deliberately retain the vanilla framing.
template <typename NativeName>
bool IsDialogCameraBlock(unsigned op, unsigned count, NativeName name) {
    if (op < 9 || count <= op || count - op < 22)
        return false;

    auto is = [&](unsigned index, const char* expected) {
        const char* actual = name(index);
        return actual && std::strcmp(actual, expected) == 0;
    };
    if (!is(op, "CameraTransit") || !is(op - 3, "StopWorld") || !is(op - 9, "IsOverrideActive"))
        return false;

    const char* expected[] = {
        "Rotate", "HasAnimationTrack", "LookAsyncCamera", "CameraWaitForPlayFinish", "ResumeWorld"};
    unsigned next = 0;
    for (unsigned offset = 1; offset <= 32 && op + offset < count; ++offset) {
        const char* actual = name(op + offset);
        if (!actual)
            continue;
        if (std::strcmp(actual, expected[next]) != 0)
            return false;
        if (++next == 5)
            return true;
    }

    return false;
}
}
