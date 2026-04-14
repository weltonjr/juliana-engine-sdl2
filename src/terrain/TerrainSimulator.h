#pragma once

#include "terrain/Terrain.h"
#include "package/DefinitionRegistry.h"
#include "package/MaterialDef.h"
#include <vector>
#include <cstring>

struct DirtyRect {
    int x, y, w, h;
};

class TerrainSimulator {
public:
    static constexpr int SIM_CHUNK = 64;  // simulation chunk size
    static constexpr int SIM_INTERVAL = 3;

    // Pressure constants
    static constexpr uint8_t MAX_MASS = 255;
    static constexpr uint8_t DEFAULT_LIQUID_MASS = 255;
    static constexpr uint8_t DEFAULT_GAS_MASS = 128;
    static constexpr int COLUMN_WEIGHT = 16;
    static constexpr int MAX_COLUMN_SCAN = 16;
    static constexpr int SPAWN_THRESHOLD = 32;
    static constexpr int MIN_TRANSFER = 2;

    TerrainSimulator(const DefinitionRegistry& registry);

    void Update(Terrain& terrain);

    // Notify that terrain was modified externally (digging, explosions)
    void NotifyModified(int rx, int ry, int rw, int rh);

    // Initialize mass overlay for liquid/gas cells in a region
    void InitMassRegion(const Terrain& terrain, int rx, int ry, int rw, int rh);

    const std::vector<DirtyRect>& GetDirtyRects() const { return dirty_rects_; }
    bool HasChanges() const { return !dirty_rects_.empty(); }

    // Chunk activity stats (for debug/HUD)
    int GetTotalChunkCount() const { return chunks_x_ * chunks_y_; }
    int GetActiveChunkCount() const {
        int n = 0;
        for (bool a : chunk_active_) if (a) ++n;
        return n;
    }

    // Access mass overlay (for debug display)
    uint8_t GetMass(int x, int y, int w) const {
        int idx = y * w + x;
        return (idx >= 0 && idx < static_cast<int>(mass_.size())) ? mass_[idx] : 0;
    }

private:
    void SimulatePowder(Terrain& terrain);
    void SimulateLiquid(Terrain& terrain, int pass);
    void SimulateGas(Terrain& terrain, int pass);
    void EqualizePressure(Terrain& terrain);
    void MarkDirty(int x, int y);
    void ScanActiveChunks(const Terrain& terrain);
    // Deactivate any currently-active chunk that no longer contains any
    // movable cell (powder with gravity, liquid, or gas). Runs once at the
    // end of Update(), after all three per-material sims have executed, so
    // it considers every material kind together.
    void PruneInactiveChunks(const Terrain& terrain);

    int EffectivePressure(const Terrain& terrain, int x, int y, int w) const;

    int ChunkIndex(int cx, int cy) const { return cy * chunks_x_ + cx; }

    const DefinitionRegistry& registry_;

    // Fast LUTs (indexed by MaterialID 0-255)
    std::vector<MaterialState> state_lut_;
    std::vector<bool> gravity_lut_;
    std::vector<int> flow_lut_;
    std::vector<int> density_lut_;
    std::vector<int> rise_rate_lut_;
    std::vector<int> dispersion_lut_;
    std::vector<int> lifetime_lut_;

    // Per-cell overlays
    std::vector<uint8_t> processed_;  // 1 = already moved this tick
    std::vector<uint8_t> mass_;       // liquid pressure / gas lifetime counter

    // Chunk-level active tracking
    int chunks_x_ = 0, chunks_y_ = 0;
    std::vector<bool> chunk_active_;   // true if chunk may contain movable cells
    bool needs_full_scan_ = true;      // first tick: scan everything

    int tick_counter_ = 0;
    int max_flow_ = 1;     // precomputed max flow_rate across all materials
    int max_rise_ = 1;     // precomputed max rise_rate across all materials

    // Dirty tracking
    int dirty_min_x_, dirty_min_y_;
    int dirty_max_x_, dirty_max_y_;
    bool any_dirty_;
    std::vector<DirtyRect> dirty_rects_;
};
