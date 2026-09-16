#include "../src/layout_selection.h"
#include <cstdio>
#include <initializer_list>

int main() {
    using namespace disco_dialogues;
    for (const auto& layout : layouts) {
        const char* selected = SelectLayout("dialog.xml", layout.width, layout.height);
        if (!selected || std::strcmp(selected, layout.xml))
            return 1;
    }

    for (const char* name : {"dialog_photo.xml", "inventory.xml", "dialog_1920x1080.xml", "not_dialog.xml"})
        if (SelectLayout(name, 1920, 1080))
            return 2;

    if (SelectLayout(nullptr, 1920, 1080) || SelectLayout("dialog.xml", 1280, 720) ||
        SelectLayout("dialog.xml", 0, 0) || SelectLayout("dialog.xml", 1920, 1200))
        return 3;

    if (LayoutLeftFraction(800, 600) != 424.0f / 800 ||
        LayoutLeftFraction(1024, 768) != 543.0f / 1024 || LayoutLeftFraction(1280, 720) != 0)
        return 4;

    std::puts("Supported sizes, unsupported sizes and unrelated windows: PASS");
    return 0;
}
