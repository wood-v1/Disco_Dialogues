#pragma once

#include <cstring>

namespace disco_dialogues {
struct Layout {
    int width;
    int height;
    const char* xml;
};

inline constexpr Layout layouts[] = {
    {1920, 1080, "disco_dialogues_1920x1080.xml"},
    {1600, 900, "disco_dialogues_1600x900.xml"},
    {1366, 768, "disco_dialogues_1366x768.xml"},
};

inline const char* SelectLayout(const char* host, int width, int height) {
    // Game.exe requests this exact name BEFORE UI.dll's resolution lookup.
    // No wildcard: dialog_photo.xml and other screens never match.
    if (!host || std::strcmp(host, "dialog.xml") != 0)
        return nullptr;

    for (const auto& layout : layouts)
        if (layout.width == width && layout.height == height)
            return layout.xml;

    return nullptr;
}
}
