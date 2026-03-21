#include "TerrainFacade.h"
#include "../core/Math.h"
#include <cmath>

TerrainFacade::TerrainFacade(int cells_w, int cells_h)
    : m_bitmap(cells_w, cells_h),
      m_chunks_x(cells_w / CHUNK_CELLS),
      m_chunks_y(cells_h / CHUNK_CELLS)
{
    m_chunks.resize(m_chunks_x * m_chunks_y);
    for (int cy = 0; cy < m_chunks_y; ++cy)
        for (int cx = 0; cx < m_chunks_x; ++cx) {
            auto& c = m_chunks[cy * m_chunks_x + cx];
            c.chunk_x = cx;
            c.chunk_y = cy;
        }
}

bool TerrainFacade::is_solid(float world_x, float world_y) const {
    auto [cx, cy] = world_to_cell(world_x, world_y);
    MaterialID mat = m_bitmap.get(cx, cy);
    return mat == MaterialID::DIRT ||
           mat == MaterialID::ROCK ||
           mat == MaterialID::GOLD_ORE;
}

MaterialID TerrainFacade::get_material(int cell_x, int cell_y) const {
    return m_bitmap.get(cell_x, cell_y);
}

MaterialID TerrainFacade::get_material_unsafe(int cell_x, int cell_y) const {
    return m_bitmap.get(cell_x, cell_y);
}

void TerrainFacade::set_material(int cell_x, int cell_y, MaterialID mat) {
    m_bitmap.set(cell_x, cell_y, mat);
    mark_chunks_dirty_for_cell(cell_x, cell_y);
}

void TerrainFacade::dig(float world_x, float world_y, float radius) {
    auto [cx, cy] = world_to_cell(world_x, world_y);
    int r = (int)(radius / CELL_SIZE) + 1;
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            float dist = std::sqrt((float)(dx*dx + dy*dy)) * CELL_SIZE;
            if (dist > radius) continue;
            int nx = cx + dx, ny = cy + dy;
            MaterialID mat = m_bitmap.get(nx, ny);
            // Rock cannot be dug in v0.1
            if (mat == MaterialID::DIRT || mat == MaterialID::GOLD_ORE) {
                m_bitmap.set(nx, ny, MaterialID::EMPTY);
                mark_chunks_dirty_for_cell(nx, ny);
            }
        }
    }
}

void TerrainFacade::damage(float wx, float wy, float radius) {
    // Same as dig for now; will be expanded in v0.2
    dig(wx, wy, radius);
}

TerrainChunk& TerrainFacade::get_chunk(int chunk_x, int chunk_y) {
    return m_chunks[chunk_y * m_chunks_x + chunk_x];
}

const TerrainChunk& TerrainFacade::get_chunk(int chunk_x, int chunk_y) const {
    return m_chunks[chunk_y * m_chunks_x + chunk_x];
}

int TerrainFacade::chunks_x() const { return m_chunks_x; }
int TerrainFacade::chunks_y() const { return m_chunks_y; }
int TerrainFacade::cells_w()  const { return m_bitmap.width(); }
int TerrainFacade::cells_h()  const { return m_bitmap.height(); }

void TerrainFacade::mark_chunks_dirty_for_cell(int cell_x, int cell_y) {
    if (!m_bitmap.in_bounds(cell_x, cell_y)) return;
    // Mark the owning chunk
    int chunk_cx = cell_x / CHUNK_CELLS;
    int chunk_cy = cell_y / CHUNK_CELLS;
    m_chunks[chunk_cy * m_chunks_x + chunk_cx].mark_dirty();

    // Also mark neighbours if cell is on a chunk boundary
    if (cell_x % CHUNK_CELLS == 0 && chunk_cx > 0)
        m_chunks[chunk_cy * m_chunks_x + (chunk_cx - 1)].mark_dirty();
    if ((cell_x + 1) % CHUNK_CELLS == 0 && chunk_cx < m_chunks_x - 1)
        m_chunks[chunk_cy * m_chunks_x + (chunk_cx + 1)].mark_dirty();
    if (cell_y % CHUNK_CELLS == 0 && chunk_cy > 0)
        m_chunks[(chunk_cy - 1) * m_chunks_x + chunk_cx].mark_dirty();
    if ((cell_y + 1) % CHUNK_CELLS == 0 && chunk_cy < m_chunks_y - 1)
        m_chunks[(chunk_cy + 1) * m_chunks_x + chunk_cx].mark_dirty();
}
