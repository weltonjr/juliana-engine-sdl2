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

bool Terrain::InBounds(int x, int y) const {
    return x >= 0 && x < width_ && y >= 0 && y < height_;
}
