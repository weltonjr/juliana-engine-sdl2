#pragma once

#include "terrain/Terrain.h"
#include "package/DefinitionRegistry.h"
#include "package/MaterialDef.h"
#include <vector>
#include <cstring>
#include <cmath>
#include <algorithm>

struct DirtyRect {
    int x, y, w, h;
};

class TerrainSimulator {
public:
    static constexpr int SIM_CHUNK = 64;

    // Velocity-based physics constants
    static constexpr float GRAVITY         = 0.5f;   // cells/tick² added to vy each tick
    static constexpr float TERMINAL_VY     = 8.0f;   // max fall speed (cells/tick)
    static constexpr float TERMINAL_VX     = 4.0f;   // max horizontal speed
    static constexpr float GAS_ANTIGRAVITY = 0.3f;   // subtracted from gas vy each tick
    static constexpr float GAS_TERMINAL_VY = -4.0f;  // max rise speed (negative = up)
    static constexpr float DIAG_KICK_VX    = 0.4f;   // horizontal impulse on diagonal slide
    static constexpr float SLEEP_SPEED_SQ  = 0.01f;  // below this squared magnitude, zero velocity

    // Pressure constants (liquid equalization)
    static constexpr uint8_t MAX_MASS           = 255;
    static constexpr uint8_t DEFAULT_LIQUID_MASS = 255;
    static constexpr uint8_t DEFAULT_GAS_MASS    = 128;
    static constexpr int     COLUMN_WEIGHT       = 16;
    static constexpr int     MAX_COLUMN_SCAN     = 16;
    static constexpr int     SPAWN_THRESHOLD     = 32;
    static constexpr int     MIN_TRANSFER        = 2;

    TerrainSimulator(const DefinitionRegistry& registry);

    void Update(Terrain& terrain);

    void NotifyModified(int rx, int ry, int rw, int rh);
    void InitMassRegion(const Terrain& terrain, int rx, int ry, int rw, int rh);

    const std::vector<DirtyRect>& GetDirtyRects() const { return dirty_rects_; }
    bool HasChanges() const { return !dirty_rects_.empty(); }

    int GetTotalChunkCount() const { return chunks_x_ * chunks_y_; }
    int GetActiveChunkCount() const {
        int n = 0;
        for (bool a : chunk_active_) if (a) ++n;
        return n;
    }

    uint8_t GetMass(int x, int y, int w) const {
        int idx = y * w + x;
        return (idx >= 0 && idx < static_cast<int>(mass_.size())) ? mass_[idx] : 0;
    }

    // Bresenham path trace result
    struct TraceResult { int x, y; bool moved; };

    // Trace a path from (src_x, src_y) toward (dst_x, dst_y) cell-by-cell.
    // Stops at the last cell for which can_displace returns true.
    // can_displace signature: bool(const Terrain&, int nx, int ny)
    template<typename CanDisplace>
    TraceResult TracePath(const Terrain& terrain,
                          int src_x, int src_y,
                          int dst_x, int dst_y,
                          int w, int h,
                          CanDisplace can_displace) const {
        int dx = dst_x - src_x;
        int dy = dst_y - src_y;
        int sx = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
        int sy = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;
        int adx = std::abs(dx);
        int ady = std::abs(dy);
        int err = adx - ady;
        int cx = src_x, cy = src_y;
        int last_x = src_x, last_y = src_y;
        bool moved = false;

        while (!(cx == dst_x && cy == dst_y)) {
            int e2 = 2 * err;
            int nx = cx, ny = cy;
            if (e2 > -ady) { err -= ady; nx += sx; }
            if (e2 <  adx) { err += adx; ny += sy; }
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) break;
            if (!can_displace(terrain, nx, ny)) break;
            last_x = nx; last_y = ny; moved = true;
            cx = nx; cy = ny;
        }
        return { last_x, last_y, moved };
    }

private:
    void SimulatePowder(Terrain& terrain);
    void SimulateLiquid(Terrain& terrain);
    void SimulateGas(Terrain& terrain);
    void EqualizePressure(Terrain& terrain);
    void MarkDirty(int x, int y);
    void ScanActiveChunks(const Terrain& terrain);
    void PruneInactiveChunks(const Terrain& terrain);
    int  EffectivePressure(const Terrain& terrain, int x, int y, int w) const;
    int  ChunkIndex(int cx, int cy) const { return cy * chunks_x_ + cx; }

    void     SwapCells(Terrain& terrain, int ax, int ay, int bx, int by, int w);
    uint32_t Xorshift32();

    // Zero velocity for a cell that became non-simulated (air, solid)
    void ZeroVelocity(int idx) {
        vel_x_[idx] = 0.0f;
        vel_y_[idx] = 0.0f;
    }

    const DefinitionRegistry& registry_;

    // Fast LUTs (indexed by MaterialID 0-255)
    std::vector<MaterialState> state_lut_;
    std::vector<bool>          gravity_lut_;
    std::vector<int>           flow_lut_;
    std::vector<int>           density_lut_;
    std::vector<int>           rise_rate_lut_;
    std::vector<int>           dispersion_lut_;
    std::vector<int>           lifetime_lut_;
    std::vector<float>         friction_lut_;
    std::vector<float>         liquid_drag_lut_;
    std::vector<float>         inertial_resistance_lut_;

    // Per-cell overlays
    std::vector<uint8_t> processed_;  // 1 = already moved this tick
    std::vector<uint8_t> mass_;       // liquid pressure / gas lifetime counter
    std::vector<float>   vel_x_;      // horizontal velocity per cell
    std::vector<float>   vel_y_;      // vertical velocity per cell (+ = down)

    // Chunk-level active tracking
    int               chunks_x_ = 0, chunks_y_ = 0;
    std::vector<bool> chunk_active_;
    bool              needs_full_scan_ = true;

    // Column shuffling
    std::vector<int> shuffle_x_;
    uint32_t         rng_state_ = 12345u;

    int tick_counter_ = 0;  // used as entropy source for shuffle

    // Dirty tracking
    int  dirty_min_x_, dirty_min_y_;
    int  dirty_max_x_, dirty_max_y_;
    bool any_dirty_;
    std::vector<DirtyRect> dirty_rects_;
};
