#pragma once

#include "core/Types.h"
#include <vector>

struct Cell {
    MaterialID material_id;
    BackgroundID background_id;
};

class Terrain {
public:
    Terrain(int width, int height);

    Cell GetCell(int x, int y) const;
    void SetCell(int x, int y, Cell cell);
    void SetMaterial(int x, int y, MaterialID mat);
    void SetBackground(int x, int y, BackgroundID bg);
    bool InBounds(int x, int y) const;

    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }

private:
    int width_;
    int height_;
    std::vector<Cell> cells_;
};
