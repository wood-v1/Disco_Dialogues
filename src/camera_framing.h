#pragma once

#include <cmath>

namespace disco_dialogues {
struct Vector3 {
    float x, y, z;
};

inline Vector3 Cross(Vector3 a, Vector3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float Dot(Vector3 a, Vector3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline bool Normalize(Vector3& v) {
    const float length = std::sqrt(Dot(v, v));
    if (!std::isfinite(length) || length < 0.00001f)
        return false;

    v = {v.x / length, v.y / length, v.z / length};
    return true;
}

inline bool FrameOnLeft(Vector3& direction, const Vector3& up, float horizontalFov, float freeFraction) {
    if (!std::isfinite(horizontalFov) || horizontalFov < 0.1f || horizontalFov > 3.0f ||
        !std::isfinite(freeFraction) || freeFraction < 0.4f || freeFraction >= 1.0f)
        return false;

    Vector3 forward = direction;
    Vector3 right = Cross(up, forward);
    if (!Normalize(forward) || !Normalize(right))
        return false;

    // A target on the old optical axis projects to freeFraction*screenWidth/2.
    // Turn the view to the right; the interlocutor moves left. Position/FOV unchanged.
    const float tangent = (1.0f - freeFraction) * std::tan(horizontalFov * 0.5f);
    Vector3 framed{
        forward.x + right.x * tangent, forward.y + right.y * tangent, forward.z + right.z * tangent};
    if (!Normalize(framed))
        return false;

    direction = framed;
    return true;
}
}
