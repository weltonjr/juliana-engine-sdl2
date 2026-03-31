#include "terrain/Terrain.h"

Terrain::Terrain(int width, int height)
    : width_(width), height_(height), cells_(width * height, Cell{0, 0})
{
}

Cell Terrain::GetCell(int x, int y) const {
    if (!InBounds(x, y)) return Cell{0, 0};
    return cells_[y * width_ + x];
}

void Terrain::SetCell(int x, int y, Cell cell) {
    if (!InBounds(x, y)) return;
    cells_[y * width_ + x] = cell;
}

void Terrain::SetMaterial(int x, int y, MaterialID mat) {
    if (!InBounds(x, y)) return;
    cells_[y * width_ + x].material_id = mat;
}

void Terrain::SetBackground(int x, int y, BackgroundID bg) {
    if (!InBounds(x, y)) return;
    cells_[y * width_ + x].background_id = bg;
}

int Terrain::DigCircle(int cx, int cy, int radius, MaterialID air_id) {
    int count = 0;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy > radius * radius) continue;
            int px = cx + dx;
            int py = cy + dy;
            if (!InBounds(px, py)) continue;
            Cell cell = GetCell(px, py);
            if (cell.material_id != air_id) {
                SetMaterial(px, py, air_id);
                count++;
            }
        }
    }
    return count;
}

int Terrain::DigRect(int x, int y, int w, int h, MaterialID air_id) {
    int count = 0;
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            if (!InBounds(px, py)) continue;
            Cell cell = GetCell(px, py);
            if (cell.material_id != air_id) {
                SetMaterial(px, py, air_id);
                count++;
            }
        }
    }
    return count;
}

bool Terrain::InBounds(int x, int y) const {
    return x >= 0 && x < width_ && y >= 0 && y < height_;
}
