#pragma once

#include "terrain/Terrain.h"
#include "package/DefinitionRegistry.h"
#include "package/MaterialDef.h"
#include <vector>

struct DirtyRect {
    int x, y, w, h;
};

class TerrainSimulator {
public:
    static constexpr int SIM_CHUNK = 64;  // simulation chunk size
    static constexpr int SIM_INTERVAL = 3;

    TerrainSimulator(const DefinitionRegistry& registry);

    void Update(Terrain& terrain);

    // Notify that terrain was modified externally (digging, explosions)
    void NotifyModified(int rx, int ry, int rw, int rh);

    const std::vector<DirtyRect>& GetDirtyRects() const { return dirty_rects_; }
    bool HasChanges() const { return !dirty_rects_.empty(); }

private:
    void SimulatePowder(Terrain& terrain);
    void SimulateLiquid(Terrain& terrain);
    void MarkDirty(int x, int y);
    void ScanActiveChunks(const Terrain& terrain);

    int ChunkIndex(int cx, int cy) const { return cy * chunks_x_ + cx; }

    const DefinitionRegistry& registry_;

    // Fast LUTs
    std::vector<MaterialState> state_lut_;
    std::vector<bool> gravity_lut_;
    std::vector<int> flow_lut_;

    // Chunk-level active tracking
    int chunks_x_ = 0, chunks_y_ = 0;
    std::vector<bool> chunk_active_;   // true if chunk may contain movable cells
    bool needs_full_scan_ = true;      // first tick: scan everything

    int tick_counter_ = 0;

    // Dirty tracking
    int dirty_min_x_, dirty_min_y_;
    int dirty_max_x_, dirty_max_y_;
    bool any_dirty_;
    std::vector<DirtyRect> dirty_rects_;
};
