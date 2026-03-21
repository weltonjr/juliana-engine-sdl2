#pragma once
#include "raylib.h"
#include "../core/Types.h"

// Per-cell deterministic brightness variation using a hash of cell coords.
// Breaks up the flat look without requiring a noise texture.
static inline Color vary_color(Color base, int cx, int cy, int range = 14) {
    unsigned int h = (unsigned int)(cx * 2654435761u ^ cy * 2246822519u);
    int v = (int)(h & 0x1F) - 16; // -16..+15
    v = v * range / 16;
    auto c8 = [](int x) -> unsigned char { return x < 0 ? 0 : x > 255 ? 255 : (unsigned char)x; };
    return { c8((int)base.r + v), c8((int)base.g + v), c8((int)base.b + v), base.a };
}

// Returns the display color for a given material at cell (cx, cy).
// Used by both TerrainRenderer (bake) and OreFragment (sprite creation).
static inline Color material_cell_color(MaterialID mat, int cx, int cy) {
    Color base;
    switch (mat) {
        case MaterialID::DIRT:     base = {125, 82,  48,  255}; break;
        case MaterialID::ROCK:     base = {92,  90,  102, 255}; break;
        case MaterialID::GOLD_ORE: base = {235, 195, 30,  255}; break;
        case MaterialID::WATER:    base = {55,  115, 195, 210}; break;
        default:                   return {255, 0, 255, 255}; // magenta = unknown
    }
    return vary_color(base, cx, cy, 14);
}
