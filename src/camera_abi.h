#pragma once
#include "hd_hooks.h"

namespace disco_dialogues::hd {
// Classic HD inserts SetViewFOV(float) at slot 9. Beta's interface is different.
// HD CCameraBase constructor calls GetImportFOV through byte offset 0x28.
inline float CameraImportFov(void* camera) {
    return Method<float(__thiscall*)(void*)>(camera, 10)(camera);
}
}
