#include "../src/camera_transition.h"
#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    using disco_dialogues::IsDialogCameraBlock;
    std::array<const char*, 60> names{};
    names[11] = "IsOverrideActive"; names[17] = "StopWorld"; names[20] = "CameraTransit";
    names[24] = "Rotate"; names[33] = "HasAnimationTrack"; names[37] = "LookAsyncCamera";
    names[39] = "CameraWaitForPlayFinish"; names[41] = "ResumeWorld";
    auto lookup = [&](unsigned i) { return names[i]; };
    if (!IsDialogCameraBlock(20, 60, lookup)) return 1;
    if (IsDialogCameraBlock(0, 60, lookup) || IsDialogCameraBlock(20, 30, lookup)) return 2;
    names[33] = nullptr; names[37] = nullptr;
    if (IsDialogCameraBlock(20, 60, lookup)) return 3; // trade
    names[33] = "HasAnimationTrack"; names[37] = "LookAsyncCamera"; names[17] = nullptr;
    if (IsDialogCameraBlock(20, 60, lookup)) return 4; // unguarded cutscene
    names[17] = "StopWorld"; names[27] = "CameraPlay";
    if (IsDialogCameraBlock(20, 60, lookup)) return 5;
    // Optional fixtures are extracted from actual installed HD BINs, not hand
    // authored expected instruction streams. Each line has 42 relative slots.
    if (argc == 2) {
        std::ifstream input(argv[1]);
        if (!input) return 6;
        std::string line;
        unsigned checked = 0;
        while (std::getline(input, line)) {
            std::istringstream stream(line);
            int expected = 0;
            std::array<std::string, 42> fixture;
            stream >> expected;
            for (auto& value : fixture) if (!(stream >> value)) return 7;
            bool result = IsDialogCameraBlock(9, 42, [&](unsigned i) {
                return fixture[i] == "-" ? nullptr : fixture[i].c_str();
            });
            if (result != (expected != 0)) return 8;
            ++checked;
        }
        if (checked < 4) return 9;
        std::printf("PASS: %u real HD camera call sites classified\n", checked);
    }
    std::puts("PASS: dialog accepted; trade, cutscene, truncated and unfamiliar blocks rejected");
}
