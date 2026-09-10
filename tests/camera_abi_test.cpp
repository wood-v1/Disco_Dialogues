#include "../src/camera_abi.h"
#include <cstdio>

struct Camera {
    void** table;
    float fov;
    unsigned reads;
    unsigned writes;
};
float __fastcall Read(Camera* camera, void*) {
    ++camera->reads;
    return camera->fov;
}
void __fastcall Write(Camera* camera, void*, float fov) {
    ++camera->writes;
    camera->fov = fov;
}
int main() {
    void* table[11]{};
    table[9] = reinterpret_cast<void*>(&Write);
    table[10] = reinterpret_cast<void*>(&Read);
    Camera camera{table, 1.25f, 0, 0};
    for (unsigned i = 0; i < 10000; ++i) {
        if (disco_dialogues::hd::CameraImportFov(&camera) != 1.25f) return 1;
    }
    if (camera.reads != 10000 || camera.writes != 0 || camera.fov != 1.25f) return 2;
    std::puts("PASS: HD slot 10 getter called 10000 times; slot 9 setter never called; FOV unchanged");
    return 0;
}
