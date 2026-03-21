#include "TerrainRenderer.h"
#include "../core/Types.h"

void TerrainRenderer::draw(const TerrainFacade& terrain, Vector2 camera_offset, int screen_w, int screen_h) const {
    // Calculate visible cell range
    int cell_x0 = (int)(camera_offset.x / CELL_SIZE);
    int cell_y0 = (int)(camera_offset.y / CELL_SIZE);
    int cell_x1 = (int)((camera_offset.x + screen_w) / CELL_SIZE) + 1;
    int cell_y1 = (int)((camera_offset.y + screen_h) / CELL_SIZE) + 1;

    // Clamp to map bounds
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
            DrawRectangle(sx, sy, CELL_SIZE, CELL_SIZE, material_color(mat));
        }
    }
}

Color TerrainRenderer::material_color(MaterialID mat) const {
    switch (mat) {
        case MaterialID::DIRT:     return {139, 100,  60, 255};
        case MaterialID::ROCK:     return {100, 100, 110, 255};
        case MaterialID::GOLD_ORE: return {220, 180,  50, 255};
        case MaterialID::WATER:    return { 60, 120, 200, 200};
        default:                   return MAGENTA;
    }
}
