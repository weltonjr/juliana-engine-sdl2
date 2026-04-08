#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <cstdint>

struct UIColor {
    uint8_t r = 255, g = 255, b = 255, a = 255;
    SDL_Color ToSDL() const { return {r, g, b, a}; }
};

// Visual theme for all UI elements.
// Loaded from skin.toml; fields have sensible defaults so a missing file still works.
struct UISkin {
    // Frame / panel
    UIColor frame_bg     = {18,  18,  30,  235};
    UIColor frame_border = {65,  65,  105, 255};

    // Button — three interaction states + text + border
    UIColor button_normal  = {45,  45,  80,  255};
    UIColor button_hover   = {65,  65,  115, 255};
    UIColor button_pressed = {30,  30,  55,  255};
    UIColor button_border  = {75,  75,  120, 255};
    UIColor button_text    = {200, 200, 230, 255};

    // Labels
    UIColor label_text  = {175, 175, 210, 255};

    // Load from a skin.toml file; returns defaults if file is missing or malformed
    static UISkin LoadFromFile(const std::string& path);
};
