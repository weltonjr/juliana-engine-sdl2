#pragma once
#include "Types.h"
#include "raylib.h"

struct Vector2i {
    int x, y;
    bool operator==(const Vector2i& o) const { return x == o.x && y == o.y; }
};

struct AABB {
    float x, y;    // top-left corner in world pixels
    float w, h;    // width, height in pixels

    float right()  const { return x + w; }
    float bottom() const { return y + h; }
    float cx()     const { return x + w * 0.5f; }
    float cy()     const { return y + h * 0.5f; }

    bool overlaps(const AABB& o) const {
        return x < o.right() && right() > o.x &&
               y < o.bottom() && bottom() > o.y;
    }
};

// Convert world pixel position to cell coordinates
inline Vector2i world_to_cell(float wx, float wy) {
    return { (int)(wx / CELL_SIZE), (int)(wy / CELL_SIZE) };
}

// Convert cell coordinates to world pixel position (top-left of cell)
inline Vector2 cell_to_world(int cx, int cy) {
    return { (float)(cx * CELL_SIZE), (float)(cy * CELL_SIZE) };
}

// Convert cell coordinates to chunk coordinates
inline Vector2i cell_to_chunk(int cx, int cy) {
    return { cx / CHUNK_CELLS, cy / CHUNK_CELLS };
}

// Clamp a float
inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
