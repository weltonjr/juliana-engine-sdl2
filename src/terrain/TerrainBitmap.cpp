#include "TerrainBitmap.h"

TerrainBitmap::TerrainBitmap(int width, int height)
    : m_width(width), m_height(height),
      m_data(width * height, static_cast<uint8_t>(MaterialID::EMPTY))
{}

bool TerrainBitmap::in_bounds(int cx, int cy) const {
    return cx >= 0 && cx < m_width && cy >= 0 && cy < m_height;
}

MaterialID TerrainBitmap::get(int cx, int cy) const {
    if (!in_bounds(cx, cy)) return MaterialID::EMPTY;
    return static_cast<MaterialID>(m_data[cy * m_width + cx]);
}

void TerrainBitmap::set(int cx, int cy, MaterialID mat) {
    if (!in_bounds(cx, cy)) return;
    m_data[cy * m_width + cx] = static_cast<uint8_t>(mat);
}
