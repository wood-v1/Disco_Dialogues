#include "../src/camera_framing.h"
#include "../src/layout_selection.h"
#include <cstdio>
#include <initializer_list>

int main() {
    using namespace disco_dialogues;
    const auto clampedHigh = ClampCameraFramingSettings(75, 25);
    const auto clampedLow = ClampCameraFramingSettings(-75, -25);
    const auto negativeSetting = ClampCameraFramingSettings(-20, 0);
    const auto unchangedDefault = ClampCameraFramingSettings(50, 0);
    const CameraFramingSettings shippedDefault{};
    if (clampedHigh.leftAreaPosition != 50 || clampedHigh.verticalRotation != 5 ||
        clampedLow.leftAreaPosition != -50 || clampedLow.verticalRotation != -5 ||
        negativeSetting.leftAreaPosition != -20 || negativeSetting.verticalRotation != 0 ||
        unchangedDefault.leftAreaPosition != 50 || unchangedDefault.verticalRotation != 0 ||
        shippedDefault.leftAreaPosition != 5 || shippedDefault.verticalRotation != 3)
        return 9;

    const float fovs[] = {0.7f, 1.0f, 1.5f, 2.0f};

    for (const auto& layout : layouts)
        for (float fov : fovs) {
            const float fraction = LayoutLeftFraction(layout.width, layout.height);
            const Vector3 original{0, 0, 1};
            const Vector3 up{0, 1, 0};
            for (int position : {-50, -25, 0, 25, 50}) {
                Vector3 direction = original;
                if (!FrameOnLeft(direction, up, fov, fraction, {position, 0}))
                    return 1;

                const Vector3 right = Cross(up, direction);
                const float ndc = Dot(original, right) / Dot(original, direction) / std::tan(fov / 2);
                const float pixel = (ndc + 1) * layout.width / 2;
                if (std::fabs(pixel - layout.panelLeft * position / 100.0f) > 0.01f || direction.x <= 0)
                    return 2;
                if (std::fabs(Dot(direction, direction) - 1) > 0.00001f)
                    return 3;

                Vector3 repeated = original;
                FrameOnLeft(repeated, up, fov, fraction, {position, 0});
                if (std::fabs(Dot(direction, repeated) - 1) > 0.00001f)
                    return 4;

                if (position == 50) {
                    Vector3 legacy = original;
                    FrameOnLeft(legacy, up, fov, fraction, {50, 0});
                    const float oldTangent = (1.0f - fraction) * std::tan(fov * 0.5f);
                    Vector3 previous{oldTangent, 0, 1};
                    Normalize(previous);
                    if (legacy.x != previous.x || legacy.y != previous.y || legacy.z != previous.z)
                        return 5;
                }
            }
            Vector3 configured = original, explicitSettings = original;
            if (!FrameOnLeft(configured, up, fov, fraction) ||
                !FrameOnLeft(explicitSettings, up, fov, fraction, {5, 3}) ||
                configured.x != explicitSettings.x || configured.y != explicitSettings.y ||
                configured.z != explicitSettings.z)
                return 10;
        }

    Vector3 level{0, 0, 1}, raised = level, lowered = level;
    if (!FrameOnLeft(level, {0, 1, 0}, 1, 0.65f, {40, 0}) ||
        !FrameOnLeft(raised, {0, 1, 0}, 1, 0.65f, {40, 5}) ||
        !FrameOnLeft(lowered, {0, 1, 0}, 1, 0.65f, {40, -5}) ||
        !(raised.y > level.y && level.y > lowered.y) ||
        std::fabs(Dot(raised, raised) - 1) > 0.00001f ||
        std::fabs(Dot(lowered, lowered) - 1) > 0.00001f)
        return 6;

    Vector3 unchanged{0, 0, 1};
    if (FrameOnLeft(unchanged, {0, 1, 0}, 1, 0) || FrameOnLeft(unchanged, {0, 1, 0}, 1, 1) ||
        FrameOnLeft(unchanged, {0, 1, 0}, 0, 0.65f) || FrameOnLeft(unchanged, {0, 0, 1}, 1, 0.65f) ||
        FrameOnLeft(unchanged, {0, 1, 0}, 1, 0.65f, {-51, 0}) ||
        FrameOnLeft(unchanged, {0, 1, 0}, 1, 0.65f, {51, 0}) ||
        FrameOnLeft(unchanged, {0, 1, 0}, 1, 0.65f, {50, 6}))
        return 7;
    if (unchanged.x != 0 || unchanged.z != 1)
        return 8;

    std::puts(
        "PASS: configurable left framing at 5 resolutions / 4 FOVs; defaults 5/3; legacy 50/0 unchanged; invalid inputs unchanged");
    return 0;
}
