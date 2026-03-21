#include "TerrainRenderer.h"
#include "../core/Types.h"

// Per-cell deterministic brightness variation — breaks up the flat look.
// Returns base color with ±range brightness applied using a hash of cell coords.
static Color vary_color(Color base, int cx, int cy, int range = 14) {
    unsigned int h = (unsigned int)(cx * 2654435761u ^ cy * 2246822519u);
    int v = (int)(h & 0x1F) - 16; // -16..+15
    v = v * range / 16;
    auto clamp8 = [](int x) -> unsigned char {
        return (unsigned char)(x < 0 ? 0 : x > 255 ? 255 : x);
    };
    return { clamp8((int)base.r + v),
             clamp8((int)base.g + v),
             clamp8((int)base.b + v),
             base.a };
}

// ── public ─────────────────────────────────────────────────────────────────

void TerrainRenderer::draw(const TerrainFacade& terrain,
                            Vector2 camera_offset,
                            int screen_w, int screen_h) const
{
    int cell_x0 = (int)(camera_offset.x / CELL_SIZE);
    int cell_y0 = (int)(camera_offset.y / CELL_SIZE);
    int cell_x1 = (int)((camera_offset.x + screen_w)  / CELL_SIZE) + 1;
    int cell_y1 = (int)((camera_offset.y + screen_h) / CELL_SIZE) + 1;

    cell_x0 = cell_x0 < 0 ? 0 : cell_x0;
    cell_y0 = cell_y0 < 0 ? 0 : cell_y0;
    cell_x1 = cell_x1 > terrain.cells_w() ? terrain.cells_w() : cell_x1;
    cell_y1 = cell_y1 > terrain.cells_h() ? terrain.cells_h() : cell_y1;

    for (int cy = cell_y0; cy < cell_y1; ++cy) {
        for (int cx = cell_x0; cx < cell_x1; ++cx) {
            MaterialID mat = terrain.get_material_unsafe(cx, cy);
            if (mat == MaterialID::EMPTY || mat == MaterialID::AIR) continue;

            int sx = (int)(cx * CELL_SIZE - camera_offset.x);
            int sy = (int)(cy * CELL_SIZE - camera_offset.y);

            Color col = cell_color(terrain, mat, cx, cy);
            DrawRectangle(sx, sy, CELL_SIZE, CELL_SIZE, col);

            // Water: draw a lighter horizontal "shimmer" line at the top pixel row
            if (mat == MaterialID::WATER) {
                DrawRectangle(sx, sy, CELL_SIZE, 1, {120, 180, 230, 180});
            }
        }
    }
}

// ── private ────────────────────────────────────────────────────────────────

Color TerrainRenderer::cell_color(const TerrainFacade& terrain,
                                   MaterialID mat, int cx, int cy) const
{
    Color base;
    switch (mat) {
        case MaterialID::DIRT:
            // Warm brown — gets slightly darker with depth via cy hash
            base = {125, 82, 48, 255};
            break;
        case MaterialID::ROCK:
            base = {92, 90, 102, 255};
            break;
        case MaterialID::GOLD_ORE:
            // Bright, saturated gold — high visibility
            base = {235, 195, 30, 255};
            break;
        case MaterialID::WATER:
            base = {55, 115, 195, 210};
            break;
        default:
            return MAGENTA;
    }

    return vary_color(base, cx, cy, 14);
}
