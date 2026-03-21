#pragma once

// Per-chunk metadata — dirty flags drive rebuild of visual and collision data.
struct TerrainChunk {
    int chunk_x = 0;       // chunk grid coordinates
    int chunk_y = 0;
    bool dirty_visual     = true;   // needs visual rebuild
    bool dirty_collision  = true;   // needs collision rebuild

    void mark_dirty() {
        dirty_visual    = true;
        dirty_collision = true;
    }
};
