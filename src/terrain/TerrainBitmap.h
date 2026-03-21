#pragma once
#include <vector>
#include <cstdint>
#include "../core/Types.h"

// Internal terrain storage — do NOT include this outside terrain/ module.
// All external access must go through TerrainFacade.
class TerrainBitmap {
public:
    TerrainBitmap(int width, int height);

    MaterialID get(int cx, int cy) const;
    void set(int cx, int cy, MaterialID mat);
    bool in_bounds(int cx, int cy) const;

    int width()  const { return m_width; }
    int height() const { return m_height; }

private:
    int m_width, m_height;
    std::vector<uint8_t> m_data;
};
