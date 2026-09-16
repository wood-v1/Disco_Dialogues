#include "../src/camera_framing.h"
#include "../src/layout_selection.h"
#include <cstdio>

int main() {
    using namespace disco_dialogues;
    const float fovs[] = {0.7f, 1.0f, 1.5f, 2.0f};

    for (const auto& layout : layouts)
        for (float fov : fovs) {
            const float fraction = LayoutLeftFraction(layout.width, layout.height);
            const Vector3 original{0, 0, 1};
            const Vector3 up{0, 1, 0};
            Vector3 direction = original;
            if (!FrameOnLeft(direction, up, fov, fraction))
                return 1;

            const Vector3 right = Cross(up, direction);
            const float ndc = Dot(original, right) / Dot(original, direction) / std::tan(fov / 2);
            const float pixel = (ndc + 1) * layout.width / 2;
            if (std::fabs(pixel - layout.panelLeft / 2.0f) > 0.01f || direction.x <= 0)
                return 2;
            if (std::fabs(Dot(direction, direction) - 1) > 0.00001f)
                return 3;

            Vector3 repeated = original;
            FrameOnLeft(repeated, up, fov, fraction);
            if (std::fabs(Dot(direction, repeated) - 1) > 0.00001f)
                return 4;
        }

    Vector3 unchanged{0, 0, 1};
    if (FrameOnLeft(unchanged, {0, 1, 0}, 1, 0) || FrameOnLeft(unchanged, {0, 1, 0}, 1, 1) ||
        FrameOnLeft(unchanged, {0, 1, 0}, 0, 0.65f) || FrameOnLeft(unchanged, {0, 0, 1}, 1, 0.65f))
        return 5;
    if (unchanged.x != 0 || unchanged.z != 1)
        return 6;

    std::puts(
        "PASS: NPC projects to center of left region at 5 resolutions / 4 FOVs; no accumulation; invalid inputs unchanged");
    return 0;
}
