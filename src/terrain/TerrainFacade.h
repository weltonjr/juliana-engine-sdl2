#pragma once
#include <vector>
#include "../core/Types.h"
#include "TerrainBitmap.h"
#include "TerrainChunk.h"

// Single public interface to the terrain system.
// No other system may access TerrainBitmap or TerrainChunk directly.
class TerrainFacade {
public:
    TerrainFacade(int cells_w, int cells_h);

    // Query
    bool       is_solid(float world_x, float world_y) const;
    MaterialID get_material(int cell_x, int cell_y) const;

    // Mutation — automatically marks affected chunks dirty
    void set_material(int cell_x, int cell_y, MaterialID mat);
    void dig(float world_x, float world_y, float radius);
    void damage(float wx, float wy, float radius);

    // Chunk access (used by renderer and collision builder)
    TerrainChunk& get_chunk(int chunk_x, int chunk_y);
    const TerrainChunk& get_chunk(int chunk_x, int chunk_y) const;
    int chunks_x() const;
    int chunks_y() const;

    // Direct cell read (used by renderer — read-only)
    MaterialID get_material_unsafe(int cell_x, int cell_y) const;

    int cells_w() const;
    int cells_h() const;

private:
    void mark_chunks_dirty_for_cell(int cell_x, int cell_y);

    TerrainBitmap m_bitmap;
    std::vector<TerrainChunk> m_chunks; // row-major: chunk_y * chunks_x + chunk_x
    int m_chunks_x, m_chunks_y;
};
