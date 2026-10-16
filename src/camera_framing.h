#pragma once

#include <algorithm>
#include <cmath>

namespace disco_dialogues {
struct Vector3 {
    float x, y, z;
};

struct CameraFramingSettings {
    static constexpr int minLeftAreaPosition = -50;
    static constexpr int maxLeftAreaPosition = 50;
    static constexpr int minVerticalRotation = -5;
    static constexpr int maxVerticalRotation = 5;

    int leftAreaPosition = 5;
    int verticalRotation = 3;
};

inline CameraFramingSettings ClampCameraFramingSettings(int leftAreaPosition, int verticalRotation) {
    return {
        std::clamp(leftAreaPosition, CameraFramingSettings::minLeftAreaPosition,
            CameraFramingSettings::maxLeftAreaPosition),
        std::clamp(verticalRotation, CameraFramingSettings::minVerticalRotation,
            CameraFramingSettings::maxVerticalRotation)};
}

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

inline bool FrameOnLeft(Vector3& direction, const Vector3& up, float horizontalFov, float freeFraction,
    CameraFramingSettings settings = {}) {
    if (!std::isfinite(horizontalFov) || horizontalFov < 0.1f || horizontalFov > 3.0f ||
        !std::isfinite(freeFraction) || freeFraction < 0.4f || freeFraction >= 1.0f ||
        settings.leftAreaPosition < CameraFramingSettings::minLeftAreaPosition ||
        settings.leftAreaPosition > CameraFramingSettings::maxLeftAreaPosition ||
        settings.verticalRotation < CameraFramingSettings::minVerticalRotation ||
        settings.verticalRotation > CameraFramingSettings::maxVerticalRotation)
        return false;

    Vector3 forward = direction;
    Vector3 right = Cross(up, forward);
    if (!Normalize(forward) || !Normalize(right))
        return false;

    // Keep the original expression at 50% so existing settings produce the same direction.
    // A smaller percentage turns the view farther right, moving the interlocutor left.
    const float target = settings.leftAreaPosition == 50 ? 1.0f - freeFraction :
        1.0f - 2.0f * freeFraction * (settings.leftAreaPosition / 100.0f);
    const float tangent = target * std::tan(horizontalFov * 0.5f);
    Vector3 framed{
        forward.x + right.x * tangent, forward.y + right.y * tangent, forward.z + right.z * tangent};
    if (!Normalize(framed))
        return false;

    if (settings.verticalRotation != 0) {
        Vector3 cameraRight = Cross(up, framed);
        if (!Normalize(cameraRight))
            return false;
        Vector3 cameraUp = Cross(framed, cameraRight);
        if (!Normalize(cameraUp))
            return false;

        constexpr float radiansPerDegree = 0.017453292519943295f;
        const float angle = settings.verticalRotation * radiansPerDegree;
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        framed = {framed.x * cosine + cameraUp.x * sine,
            framed.y * cosine + cameraUp.y * sine,
            framed.z * cosine + cameraUp.z * sine};
        if (!Normalize(framed))
            return false;
    }

    direction = framed;
    return true;
}
}
