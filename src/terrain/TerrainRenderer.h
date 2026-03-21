#pragma once
#include "TerrainFacade.h"
#include "raylib.h"

// Renders terrain using colored rectangles per cell (v0.1).
// Only cells within the camera viewport are drawn.
// Architecture note: this class is intentionally isolated so the
// rectangle-based approach can later be swapped for marching squares
// chunk textures without changing any other system.
class TerrainRenderer {
public:
    void draw(const TerrainFacade& terrain, Vector2 camera_offset, int screen_w, int screen_h) const;

private:
    Color material_color(MaterialID mat) const;
};
