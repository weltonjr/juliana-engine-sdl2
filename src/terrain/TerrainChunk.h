#pragma once
#include "raylib.h"

// Per-chunk metadata — dirty flags drive rebuild of visual and collision data.
struct TerrainChunk {
    int chunk_x = 0;       // chunk grid coordinates
    int chunk_y = 0;
    bool dirty_visual     = true;   // needs visual rebuild
    bool dirty_collision  = true;   // needs collision rebuild

    // GPU texture baked from the chunk's cells (CHUNK_CELLS × CHUNK_CELLS texels,
    // drawn scaled to CHUNK_PX × CHUNK_PX on screen). Rebuilt when dirty_visual.
    Texture2D tex       = {};
    bool      tex_valid = false;

    void mark_dirty() {
        dirty_visual    = true;
        dirty_collision = true;
    }
};
