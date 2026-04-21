#pragma once

#include "terrain/Terrain.h"
#include "package/DefinitionRegistry.h"
#include "package/MaterialDef.h"
#include <vector>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <functional>

struct SimCell; // forward-declared so TerrainSimulator can store callbacks without including SimCell.h

struct DirtyRect {
    int x, y, w, h;
};

// Pending cell mutation queued during simulation passes and applied at end of tick.
struct PendingMutation {
    enum class Type { Convert, Damage, TempChange, Ignite, Extinguish } type;
    int   x = 0, y = 0;
    int   material_id = -1; // for Convert (-1 = Air)
    int   amount      = 0;  // for Damage
    float delta       = 0;  // for TempChange
};

class TerrainSimulator {
public:
    static constexpr int SIM_CHUNK = 64;

    // Velocity-based physics constants
    static constexpr float GRAVITY         = 0.5f;
    static constexpr float TERMINAL_VY     = 8.0f;
    static constexpr float TERMINAL_VX     = 4.0f;
    static constexpr float GAS_ANTIGRAVITY = 0.3f;
    static constexpr float GAS_TERMINAL_VY = -4.0f;
    static constexpr float DIAG_KICK_VX    = 0.4f;
    static constexpr float SLEEP_SPEED_SQ  = 0.01f;

    // Temperature constants
    static constexpr float AMBIENT_PULL    = 0.005f; // fraction per tick toward ambient_temp
    static constexpr float MIN_CONDUCT_DT  = 0.01f;  // ignore sub-threshold heat transfers

    // Legacy mass constants (kept for gas lifetime tracking)
    static constexpr uint8_t MAX_MASS           = 255;
    static constexpr uint8_t DEFAULT_LIQUID_MASS = 255;
    static constexpr uint8_t DEFAULT_GAS_MASS    = 128;
    static constexpr int     COLUMN_WEIGHT       = 16;
    static constexpr int     MAX_COLUMN_SCAN     = 16;
    static constexpr int     SPAWN_THRESHOLD     = 32;
    static constexpr int     MIN_TRANSFER        = 2;

    TerrainSimulator(const DefinitionRegistry& registry);

    void Update(Terrain& terrain);

    // Trigger an explosion centered at (cx,cy) with given radius and strength.
    // Uses deterministic Xorshift32; safe for multiplayer lockstep.
    void TriggerExplosion(Terrain& terrain, int cx, int cy, int radius, int strength);

    // Apply damage to a solid cell, propagating cracks radially (for tools/weapons).
    void ApplyCrackDamage(Terrain& terrain, int x, int y, int damage);

    void NotifyModified(int rx, int ry, int rw, int rh);
    void InitMassRegion(const Terrain& terrain, int rx, int ry, int rw, int rh);

    // Register an on_tick callback for a given runtime material ID.
    // Called by the scripting system after loading material scripts.
    void SetOnTickCallback(int mat_id, std::function<void(SimCell&)> fn) {
        if (mat_id >= 0 && mat_id < 256) on_tick_cbs_[mat_id] = std::move(fn);
    }

    const std::vector<DirtyRect>& GetDirtyRects() const { return dirty_rects_; }
    bool HasChanges() const { return !dirty_rects_.empty(); }

    int GetTotalChunkCount() const { return chunks_x_ * chunks_y_; }
    int GetActiveChunkCount() const {
        int n = 0; for (bool a : chunk_active_) if (a) ++n; return n;
    }

    // ── Per-cell overlay accessors (safe: bounds-checked) ─────────────────────
    float GetTemp(int x, int y, int w) const {
        int idx = y * w + x;
        return (idx >= 0 && idx < static_cast<int>(temp_.size())) ? temp_[idx] : 0.f;
    }
    int16_t GetHealth(int x, int y, int w) const {
        int idx = y * w + x;
        return (idx >= 0 && idx < static_cast<int>(health_.size())) ? health_[idx] : 0;
    }
    bool IsIgnited(int x, int y, int w) const {
        int idx = y * w + x;
        return (idx >= 0 && idx < static_cast<int>(ignited_.size())) && ignited_[idx] != 0;
    }
    uint8_t GetCrack(int x, int y, int w) const {
        int idx = y * w + x;
        return (idx >= 0 && idx < static_cast<int>(crack_.size())) ? crack_[idx] : 0;
    }

    // Per-cell setters (bounds-checked)
    void SetTemp(int x, int y, int w, float t) {
        int idx = y * w + x;
        if (idx >= 0 && idx < static_cast<int>(temp_.size())) temp_[idx] = t;
    }
    void SetHealth(int x, int y, int w, int16_t hp) {
        int idx = y * w + x;
        if (idx >= 0 && idx < static_cast<int>(health_.size())) health_[idx] = hp;
    }
    void SetIgnited(int x, int y, int w, bool on) {
        int idx = y * w + x;
        if (idx >= 0 && idx < static_cast<int>(ignited_.size())) ignited_[idx] = on ? 1 : 0;
    }

    // Raw overlay pointers for FragmentTracker / renderer overlays
    uint8_t*       GetCrackOverlay()       { return crack_.empty()  ? nullptr : crack_.data(); }
    const float*   GetTempOverlay()  const { return temp_.empty()   ? nullptr : temp_.data(); }
    const int16_t* GetHealthOverlay()const { return health_.empty() ? nullptr : health_.data(); }

    // Runtime LUT mutation (for Lua/editor toggles)
    void SetConductsHeatLUT(int runtime_id, bool on) {
        if (runtime_id >= 0 && runtime_id < static_cast<int>(conducts_heat_lut_.size()))
            conducts_heat_lut_[runtime_id] = on;
    }

    // Spawn an ephemeral particle at (x,y) with velocity and lifetime override.
    void SpawnParticleAt(int x, int y, int w, float vx, float vy, int ttl);

    // Stain overlay accessors for rendering
    uint8_t GetStainR(int idx) const { return idx < (int)stain_r_.size() ? stain_r_[idx] : 0; }
    uint8_t GetStainG(int idx) const { return idx < (int)stain_g_.size() ? stain_g_[idx] : 0; }
    uint8_t GetStainB(int idx) const { return idx < (int)stain_b_.size() ? stain_b_[idx] : 0; }
    uint8_t GetStainA(int idx) const { return idx < (int)stain_a_.size() ? stain_a_[idx] : 0; }

    // ── Material callback registration (on_contact, on_heat) ─────────────────
    void SetOnContactCallback(int mat_id, std::function<void(SimCell&, SimCell&)> fn) {
        if (mat_id >= 0 && mat_id < 256) on_contact_cbs_[mat_id] = std::move(fn);
    }
    void SetOnHeatCallback(int mat_id, std::function<void(SimCell&, float)> fn) {
        if (mat_id >= 0 && mat_id < 256) on_heat_cbs_[mat_id] = std::move(fn);
    }

    // Bresenham path trace result
    struct TraceResult { int x, y; bool moved; };

    template<typename CanDisplace>
    TraceResult TracePath(const Terrain& terrain,
                          int src_x, int src_y,
                          int dst_x, int dst_y,
                          int w, int h,
                          CanDisplace can_displace) const {
        int dx = dst_x - src_x, dy = dst_y - src_y;
        int sx = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
        int sy = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;
        int adx = std::abs(dx), ady = std::abs(dy);
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
    void SimulateTemperature(Terrain& terrain);
    void SimulateHealth(Terrain& terrain);
    void SimulateSpecial(Terrain& terrain);
    void DispatchLuaCallbacks(Terrain& terrain);
    void ApplyPendingMutations(Terrain& terrain);

    void MarkDirty(int x, int y);
    void ScanActiveChunks(const Terrain& terrain);
    void PruneInactiveChunks(const Terrain& terrain);
    int  ChunkIndex(int cx, int cy) const { return cy * chunks_x_ + cx; }

    void     SwapCells(Terrain& terrain, int ax, int ay, int bx, int by, int w);
    uint32_t Xorshift32();

    // Convert a cell to a new material, preserving velocity and clearing reactive state.
    void ConvertCell(Terrain& terrain, int x, int y, int w, int new_mat_id);

    // Apply stain color to a cell, blending with existing stain.
    void ApplyStain(int idx, uint8_t r, uint8_t g, uint8_t b, float strength);

    void ZeroVelocity(int idx) {
        vel_x_[idx] = vel_y_[idx] = 0.0f;
        sleeping_[idx] = 0;
    }

    const DefinitionRegistry& registry_;

    // ── Fast LUTs (indexed by MaterialID 0-255) ───────────────────────────────
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
    std::vector<int>           blast_resistance_lut_;
    // Temperature
    std::vector<float>         heat_conductivity_lut_;
    std::vector<float>         ambient_temp_lut_;
    std::vector<float>         combustion_heat_lut_;
    std::vector<float>         ignition_temp_lut_;
    std::vector<bool>          conducts_heat_lut_;
    // Phase changes: rules keyed by direction (nested vectors per material ID)
    std::vector<std::vector<std::pair<float,int>>> phase_above_lut_; // temp > thr → mat_id
    std::vector<std::vector<std::pair<float,int>>> phase_below_lut_; // temp < thr → mat_id
    // Health
    std::vector<int>           max_health_lut_;
    std::vector<int>           death_product_lut_;  // runtime id, -1 = Air
    std::vector<int>           corrode_damage_lut_;
    std::vector<bool>          corrode_self_lut_;
    // Solidification
    std::vector<int>           solidify_ticks_lut_;
    std::vector<int>           solidify_into_lut_;  // runtime id, -1 = none
    // Stain
    std::vector<float>         stain_strength_lut_;
    std::vector<float>         stain_fade_lut_;
    std::vector<uint8_t>       stain_r_lut_, stain_g_lut_, stain_b_lut_;

    // ── Per-cell overlays (all resize-not-assign: state persists across ticks) ─
    std::vector<uint8_t> processed_;   // 1 = moved this tick
    std::vector<uint8_t> mass_;        // gas lifetime counter
    std::vector<float>   vel_x_;
    std::vector<float>   vel_y_;
    std::vector<uint8_t> sleeping_;    // 1 = powder settled; woken by SwapCells on neighbor
    std::vector<float>   temp_;        // temperature per cell (°C)
    std::vector<int16_t> health_;      // current health (negative = dead)
    std::vector<uint8_t> ignited_;     // 1 = burning
    std::vector<int>     stationary_;  // ticks unmoved (solidification counter)
    std::vector<uint8_t> stain_r_, stain_g_, stain_b_; // stain color channels
    std::vector<uint8_t> stain_a_;     // stain alpha (0 = no stain)
    std::vector<uint8_t> crack_;       // crack intensity per cell (0=intact, 255=fully cracked)

    // Pending mutations accumulated during simulation passes
    std::vector<PendingMutation> pending_;

    // Per-material Lua callbacks (indexed by runtime MaterialID 0-255)
    std::function<void(SimCell&)>              on_tick_cbs_[256]    = {};
    std::function<void(SimCell&, SimCell&)>    on_contact_cbs_[256] = {};
    std::function<void(SimCell&, float)>       on_heat_cbs_[256]    = {};

    // ── Chunk tracking ────────────────────────────────────────────────────────
    int               chunks_x_ = 0, chunks_y_ = 0;
    std::vector<bool> chunk_active_;
    bool              needs_full_scan_ = true;

    std::vector<int> shuffle_x_;
    uint32_t         rng_state_ = 12345u;
    int              tick_counter_ = 0;

    // ── Dirty tracking ────────────────────────────────────────────────────────
    int  dirty_min_x_, dirty_min_y_;
    int  dirty_max_x_, dirty_max_y_;
    bool any_dirty_;
    std::vector<DirtyRect> dirty_rects_;
};
