#include "terrain/TerrainSimulator.h"
#include "scripting/SimCell.h"
#include "package/MaterialDef.h"
#include <algorithm>
#include <cstring>
#include <cmath>

// ── Constructor ───────────────────────────────────────────────────────────────

TerrainSimulator::TerrainSimulator(const DefinitionRegistry& registry)
    : registry_(registry)
    , dirty_min_x_(0), dirty_min_y_(0)
    , dirty_max_x_(0), dirty_max_y_(0)
    , any_dirty_(false)
    , needs_full_scan_(true)
    , tick_counter_(0)
{
    // Core physics LUTs
    state_lut_.resize(256, MaterialState::None);
    gravity_lut_.resize(256, false);
    flow_lut_.resize(256, 0);
    density_lut_.resize(256, 0);
    rise_rate_lut_.resize(256, 0);
    dispersion_lut_.resize(256, 0);
    lifetime_lut_.resize(256, 0);
    friction_lut_.resize(256, 0.8f);
    liquid_drag_lut_.resize(256, 0.0f);
    inertial_resistance_lut_.resize(256, 0.0f);
    blast_resistance_lut_.resize(256, 20);
    // Temperature LUTs
    heat_conductivity_lut_.resize(256, 0.05f);
    ambient_temp_lut_.resize(256, 0.0f);
    combustion_heat_lut_.resize(256, 0.0f);
    ignition_temp_lut_.resize(256, -1.0f);
    conducts_heat_lut_.resize(256, true);
    phase_above_lut_.resize(256);
    phase_below_lut_.resize(256);
    // Health LUTs
    max_health_lut_.resize(256, 0);
    death_product_lut_.resize(256, -1);
    corrode_damage_lut_.resize(256, 0);
    corrode_self_lut_.resize(256, false);
    // Solidification LUTs
    solidify_ticks_lut_.resize(256, 0);
    solidify_into_lut_.resize(256, -1);
    // Stain LUTs
    stain_strength_lut_.resize(256, 0.0f);
    stain_fade_lut_.resize(256, 0.005f);
    stain_r_lut_.resize(256, 0);
    stain_g_lut_.resize(256, 0);
    stain_b_lut_.resize(256, 0);

    for (int i = 0; i < 256; i++) {
        auto* mat = registry.GetMaterialByRuntimeID(static_cast<MaterialID>(i));
        if (!mat) continue;

        state_lut_[i]               = mat->state;
        gravity_lut_[i]             = mat->gravity;
        flow_lut_[i]                = mat->flow_rate;
        density_lut_[i]             = mat->density;
        rise_rate_lut_[i]           = mat->rise_rate;
        dispersion_lut_[i]          = mat->dispersion;
        lifetime_lut_[i]            = mat->lifetime;
        friction_lut_[i]            = mat->friction;
        liquid_drag_lut_[i]         = mat->liquid_drag;
        inertial_resistance_lut_[i] = mat->inertial_resistance;
        blast_resistance_lut_[i]    = mat->blast_resistance;
        heat_conductivity_lut_[i]   = mat->heat_conductivity;
        ambient_temp_lut_[i]        = mat->ambient_temp;
        combustion_heat_lut_[i]     = mat->combustion_heat;
        ignition_temp_lut_[i]       = mat->ignition_temp;
        conducts_heat_lut_[i]       = mat->conducts_heat;
        max_health_lut_[i]          = mat->max_health;
        corrode_damage_lut_[i]      = mat->corrode_damage;
        corrode_self_lut_[i]        = mat->corrode_self;
        solidify_ticks_lut_[i]      = mat->solidify_ticks;
        stain_strength_lut_[i]      = mat->stain_strength;
        stain_fade_lut_[i]          = mat->stain_fade_rate;
        stain_r_lut_[i]             = mat->stain_color.r;
        stain_g_lut_[i]             = mat->stain_color.g;
        stain_b_lut_[i]             = mat->stain_color.b;

        // Resolve qualified string IDs to runtime IDs
        if (!mat->death_product.empty()) {
            auto* dp = registry.GetMaterial(mat->death_product);
            death_product_lut_[i] = dp ? static_cast<int>(dp->runtime_id) : -1;
        }
        if (!mat->solidify_into.empty()) {
            auto* si = registry.GetMaterial(mat->solidify_into);
            solidify_into_lut_[i] = si ? static_cast<int>(si->runtime_id) : -1;
        }

        // Build phase change LUTs (resolve qualified ids)
        for (const auto& rule : mat->phase_changes) {
            auto* target = registry.GetMaterial(rule.into);
            if (!target) continue;
            int tid = static_cast<int>(target->runtime_id);
            if (rule.above)
                phase_above_lut_[i].emplace_back(rule.threshold, tid);
            else
                phase_below_lut_[i].emplace_back(rule.threshold, tid);
        }
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

uint32_t TerrainSimulator::Xorshift32() {
    rng_state_ ^= rng_state_ << 13;
    rng_state_ ^= rng_state_ >> 17;
    rng_state_ ^= rng_state_ << 5;
    return rng_state_;
}

void TerrainSimulator::SwapCells(Terrain& terrain, int ax, int ay, int bx, int by, int w) {
    Cell ca = terrain.GetCell(ax, ay);
    Cell cb = terrain.GetCell(bx, by);
    terrain.SetCell(bx, by, ca);
    terrain.SetCell(ax, ay, cb);

    int ia = ay * w + ax;
    int ib = by * w + bx;
    std::swap(mass_[ia],    mass_[ib]);
    std::swap(vel_x_[ia],   vel_x_[ib]);
    std::swap(vel_y_[ia],   vel_y_[ib]);
    std::swap(sleeping_[ia], sleeping_[ib]);

    // Wake lateral neighbors of both endpoints so settled powder reacts
    int h = static_cast<int>(sleeping_.size()) / w;
    auto wake = [&](int wx, int wy) {
        if (wx >= 0 && wx < w && wy >= 0 && wy < h)
            sleeping_[wy * w + wx] = 0;
    };
    wake(ax - 1, ay); wake(ax + 1, ay);
    wake(bx - 1, by); wake(bx + 1, by);

    MarkDirty(ax, ay);
    MarkDirty(bx, by);

    // Fire on_contact callbacks when two different materials meet during a swap.
    if (ca.material_id != cb.material_id) {
        int h2 = static_cast<int>(temp_.size()) / w;
        auto fire_contact = [&](int mat, int sx, int sy, int ox, int oy) {
            if (mat < 0 || mat >= 256 || !on_contact_cbs_[mat]) return;
            SimCell sc_self, sc_other;
            sc_self.terrain = &terrain; sc_self.x = sx; sc_self.y = sy;
            sc_self.w = w; sc_self.h = h2;
            sc_self.temp = temp_.data(); sc_self.health = health_.data();
            sc_self.ignited = ignited_.data(); sc_self.pending = &pending_;
            sc_self.registry = &registry_;
            sc_other = sc_self; sc_other.x = ox; sc_other.y = oy;
            on_contact_cbs_[mat](sc_self, sc_other);
        };
        // After swap: ca is now at (bx,by), cb is now at (ax,ay)
        fire_contact(ca.material_id, bx, by, ax, ay);
        fire_contact(cb.material_id, ax, ay, bx, by);
    }
}

void TerrainSimulator::MarkDirty(int x, int y) {
    if (!any_dirty_) {
        dirty_min_x_ = dirty_max_x_ = x;
        dirty_min_y_ = dirty_max_y_ = y;
        any_dirty_ = true;
    } else {
        dirty_min_x_ = std::min(dirty_min_x_, x);
        dirty_min_y_ = std::min(dirty_min_y_, y);
        dirty_max_x_ = std::max(dirty_max_x_, x);
        dirty_max_y_ = std::max(dirty_max_y_, y);
    }

    int cx = x / SIM_CHUNK;
    int cy = y / SIM_CHUNK;
    if (cx >= 0 && cx < chunks_x_ && cy >= 0 && cy < chunks_y_) {
        chunk_active_[ChunkIndex(cx, cy)] = true;
        if (cy + 1 < chunks_y_) chunk_active_[ChunkIndex(cx, cy + 1)] = true;
        if (cy > 0)             chunk_active_[ChunkIndex(cx, cy - 1)] = true;
        if (cx > 0)             chunk_active_[ChunkIndex(cx - 1, cy)] = true;
        if (cx + 1 < chunks_x_) chunk_active_[ChunkIndex(cx + 1, cy)] = true;
    }
}

void TerrainSimulator::NotifyModified(int rx, int ry, int rw, int rh) {
    if (chunks_x_ == 0 || chunks_y_ == 0 || chunk_active_.empty()) return;

    int cx0 = std::max(0, (rx - 1) / SIM_CHUNK);
    int cy0 = std::max(0, (ry - 1) / SIM_CHUNK);
    int cx1 = std::min(chunks_x_ - 1, (rx + rw) / SIM_CHUNK);
    int cy1 = std::min(chunks_y_ - 1, (ry + rh + SIM_CHUNK) / SIM_CHUNK);

    for (int cy = cy0; cy <= cy1; cy++)
        for (int cx = cx0; cx <= cx1; cx++)
            chunk_active_[ChunkIndex(cx, cy)] = true;
}

void TerrainSimulator::InitMassRegion(const Terrain& terrain, int rx, int ry, int rw, int rh) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();
    int total = w * h;
    if (static_cast<int>(mass_.size()) != total) return;

    int x1 = std::min(rx + rw, w);
    int y1 = std::min(ry + rh, h);

    for (int y = std::max(0, ry); y < y1; y++) {
        for (int x = std::max(0, rx); x < x1; x++) {
            Cell cell = terrain.GetCell(x, y);
            MaterialState st = state_lut_[cell.material_id];
            int idx = y * w + x;
            if (st == MaterialState::Liquid) {
                mass_[idx] = DEFAULT_LIQUID_MASS;
            } else if (st == MaterialState::Gas) {
                int lt = lifetime_lut_[cell.material_id];
                mass_[idx] = (lt > 0) ? static_cast<uint8_t>(std::min(lt, 255)) : DEFAULT_GAS_MASS;
            } else {
                mass_[idx] = 0;
            }
        }
    }
}

void TerrainSimulator::ScanActiveChunks(const Terrain& terrain) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();
    chunks_x_ = (w + SIM_CHUNK - 1) / SIM_CHUNK;
    chunks_y_ = (h + SIM_CHUNK - 1) / SIM_CHUNK;
    chunk_active_.assign(chunks_x_ * chunks_y_, false);

    int total = w * h;
    processed_.resize(total, 0);
    mass_.resize(total, 0);
    vel_x_.assign(total, 0.0f);
    vel_y_.assign(total, 0.0f);
    // All new overlays: resize not assign — state persists across ticks
    sleeping_.resize(total, 0);
    temp_.resize(total, 0.0f);
    health_.resize(total, 0);
    ignited_.resize(total, 0);
    stationary_.resize(total, 0);
    stain_r_.resize(total, 0);
    stain_g_.resize(total, 0);
    stain_b_.resize(total, 0);
    stain_a_.resize(total, 0);
    crack_.resize(total, 0);
    shuffle_x_.resize(SIM_CHUNK);

    for (int cy = 0; cy < chunks_y_; cy++) {
        for (int cx = 0; cx < chunks_x_; cx++) {
            int x0 = cx * SIM_CHUNK;
            int y0 = cy * SIM_CHUNK;
            int x1 = std::min(x0 + SIM_CHUNK, w);
            int y1 = std::min(y0 + SIM_CHUNK, h);

            bool active = false;
            for (int y = y0; y < y1 && !active; y++) {
                for (int x = x0; x < x1 && !active; x++) {
                    Cell cell = terrain.GetCell(x, y);
                    MaterialState st = state_lut_[cell.material_id];
                    if ((st == MaterialState::Powder || st == MaterialState::Liquid ||
                         st == MaterialState::Gas) &&
                        (gravity_lut_[cell.material_id] || st == MaterialState::Gas)) {
                        active = true;
                    }
                }
            }
            chunk_active_[ChunkIndex(cx, cy)] = active;
        }
    }

    InitMassRegion(terrain, 0, 0, w, h);
}

// ── Update ────────────────────────────────────────────────────────────────────

void TerrainSimulator::Update(Terrain& terrain) {
    any_dirty_ = false;
    dirty_rects_.clear();

    tick_counter_++;
    // Seed the RNG with the tick counter each frame for varied shuffle patterns
    rng_state_ ^= static_cast<uint32_t>(tick_counter_) * 2654435761u;

    if (needs_full_scan_) {
        ScanActiveChunks(terrain);
        needs_full_scan_ = false;
    }

    int w = terrain.GetWidth();
    int h = terrain.GetHeight();
    int total = w * h;

    // Ensure overlays are sized correctly (terrain dimensions may change)
    if (static_cast<int>(processed_.size()) != total) {
        processed_.resize(total, 0);
    }
    if (static_cast<int>(mass_.size()) != total) {
        mass_.resize(total, 0);
    }
    if (static_cast<int>(vel_x_.size()) != total) {
        vel_x_.assign(total, 0.0f);
        vel_y_.assign(total, 0.0f);
    }
    auto ensure = [&](auto& v, auto def) {
        if (static_cast<int>(v.size()) != total) v.resize(total, def);
    };
    ensure(sleeping_,   (uint8_t)0);
    ensure(temp_,       0.0f);
    ensure(health_,     (int16_t)0);
    ensure(ignited_,    (uint8_t)0);
    ensure(stationary_, 0);
    ensure(stain_r_,    (uint8_t)0);
    ensure(stain_g_,    (uint8_t)0);
    ensure(stain_b_,    (uint8_t)0);
    ensure(stain_a_,    (uint8_t)0);
    ensure(crack_,      (uint8_t)0);

    // Clear processed flags each tick
    std::memset(processed_.data(), 0, processed_.size());

    SimulatePowder(terrain);
    SimulateLiquid(terrain);
    SimulateGas(terrain);
    SimulateTemperature(terrain);
    SimulateHealth(terrain);
    SimulateSpecial(terrain);
    DispatchLuaCallbacks(terrain);
    ApplyPendingMutations(terrain);
    PruneInactiveChunks(terrain);

    if (any_dirty_) {
        dirty_rects_.push_back({
            std::max(0, dirty_min_x_ - 1),
            std::max(0, dirty_min_y_ - 1),
            std::min(terrain.GetWidth(),  dirty_max_x_ + 2) - std::max(0, dirty_min_x_ - 1),
            std::min(terrain.GetHeight(), dirty_max_y_ + 2) - std::max(0, dirty_min_y_ - 1)
        });
    }
}

// ── Column Shuffle ────────────────────────────────────────────────────────────
// Fills shuffle_x_ with [x0..x1) in a random order using Fisher-Yates.
// Returns the count of columns written.
static void ShuffleColumns(std::vector<int>& buf, int x0, int x1, uint32_t& rng) {
    int n = x1 - x0;
    for (int i = 0; i < n; i++) buf[i] = x0 + i;
    for (int i = n - 1; i > 0; i--) {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        int j = static_cast<int>(rng % static_cast<uint32_t>(i + 1));
        std::swap(buf[i], buf[j]);
    }
}

// ── Powder Simulation ─────────────────────────────────────────────────────────

void TerrainSimulator::SimulatePowder(Terrain& terrain) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    for (int cy = chunks_y_ - 1; cy >= 0; cy--) {
        for (int cx = 0; cx < chunks_x_; cx++) {
            if (!chunk_active_[ChunkIndex(cx, cy)]) continue;

            int x0 = cx * SIM_CHUNK;
            int y0 = cy * SIM_CHUNK;
            int x1 = std::min(x0 + SIM_CHUNK, w);
            int y1 = std::min(y0 + SIM_CHUNK, h - 1);

            ShuffleColumns(shuffle_x_, x0, x1, rng_state_);
            int n_cols = x1 - x0;

            for (int y = y1 - 1; y >= y0; y--) {
                for (int xi = 0; xi < n_cols; xi++) {
                    int x = shuffle_x_[xi];
                    int idx = y * w + x;
                    if (processed_[idx]) continue;
                    if (sleeping_[idx])  continue;

                    Cell cell = terrain.GetCell(x, y);
                    if (state_lut_[cell.material_id] != MaterialState::Powder ||
                        !gravity_lut_[cell.material_id]) continue;

                    int cell_density = density_lut_[cell.material_id];
                    float fric = friction_lut_[cell.material_id];

                    // Gravity accumulation
                    vel_y_[idx] = std::min(vel_y_[idx] + GRAVITY, TERMINAL_VY);

                    // Convert to integer step (sub-cell remainder stays in overlay)
                    int dy = static_cast<int>(vel_y_[idx]);
                    vel_y_[idx] -= static_cast<float>(dy);
                    int dx = static_cast<int>(vel_x_[idx]);
                    vel_x_[idx] -= static_cast<float>(dx);

                    if (dy == 0 && dx == 0) continue;  // sub-cell accumulation, wait

                    // What this powder can displace
                    auto can_displace = [&](const Terrain& t, int nx, int ny) -> bool {
                        Cell nc = t.GetCell(nx, ny);
                        MaterialState ns = state_lut_[nc.material_id];
                        return ns == MaterialState::None
                            || ns == MaterialState::Gas
                            || (ns == MaterialState::Liquid &&
                                cell_density > density_lut_[nc.material_id]);
                    };

                    TraceResult tr = TracePath(terrain, x, y, x + dx, y + dy, w, h, can_displace);

                    if (tr.moved) {
                        bool landed = (tr.x != x + dx || tr.y != y + dy);
                        int didx = tr.y * w + tr.x;
                        SwapCells(terrain, x, y, tr.x, tr.y, w);
                        processed_[didx] = 1;
                        if (landed) {
                            vel_y_[didx]  = 0.0f;
                            vel_x_[didx] *= fric;
                        }
                    } else {
                        // Blocked going straight — inertial resistance before trying diagonal
                        float resistance = inertial_resistance_lut_[cell.material_id];
                        if (resistance > 0.0f) {
                            uint32_t r = Xorshift32();
                            float roll = static_cast<float>(r & 0xFFFF) / 65535.0f;
                            if (roll < resistance) {
                                // Latch into sleeping state — skip until a neighbor disturbs it
                                vel_x_[idx] = 0.0f;
                                vel_y_[idx] = 0.0f;
                                sleeping_[idx] = 1;
                                continue;
                            }
                        }
                        int pref = ((x + y) & 1) ? 1 : -1;
                        bool moved_diag = false;
                        for (int pass = 0; pass < 2 && !moved_diag; pass++) {
                            int d = (pass == 0) ? pref : -pref;
                            int nx = x + d;
                            if (nx < 0 || nx >= w) continue;

                            Cell diag = terrain.GetCell(nx, y + 1);
                            Cell side = terrain.GetCell(nx, y);
                            MaterialState diag_st = state_lut_[diag.material_id];
                            MaterialState side_st = state_lut_[side.material_id];

                            bool diag_ok = (diag_st == MaterialState::None ||
                                           diag_st == MaterialState::Gas ||
                                           (diag_st == MaterialState::Liquid &&
                                            cell_density > density_lut_[diag.material_id]));
                            bool side_ok = (side_st == MaterialState::None ||
                                           side_st == MaterialState::Gas ||
                                           side_st == MaterialState::Liquid);

                            if (diag_ok && side_ok) {
                                int didx = (y + 1) * w + nx;
                                if (!processed_[didx]) {
                                    SwapCells(terrain, x, y, nx, y + 1, w);
                                    processed_[didx] = 1;
                                    // Horizontal kick in direction of diagonal slide
                                    vel_x_[didx] = static_cast<float>(d) * DIAG_KICK_VX;
                                    vel_y_[didx] = 0.0f;
                                    moved_diag = true;
                                }
                            }
                        }

                        if (!moved_diag) {
                            // Fully blocked — settle velocities
                            vel_x_[idx] = 0.0f;
                            vel_y_[idx] = 0.0f;
                        }
                    }
                }
            }
        }
    }
}

// ── Liquid Simulation ─────────────────────────────────────────────────────────
// Vertical movement is velocity-based (gravity accumulates, Bresenham trace for
// multi-cell fall). Horizontal spreading uses a simple 1-cell-per-tick scan,
// like FallingSandJava, to avoid the oscillation and multiplication caused by
// large per-tick horizontal jumps.

void TerrainSimulator::SimulateLiquid(Terrain& terrain) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    for (int cy = chunks_y_ - 1; cy >= 0; cy--) {
        for (int cx = 0; cx < chunks_x_; cx++) {
            if (!chunk_active_[ChunkIndex(cx, cy)]) continue;

            int x0 = cx * SIM_CHUNK;
            int y0 = cy * SIM_CHUNK;
            int x1 = std::min(x0 + SIM_CHUNK, w);
            int y1 = std::min(y0 + SIM_CHUNK, h - 1);

            ShuffleColumns(shuffle_x_, x0, x1, rng_state_);
            int n_cols = x1 - x0;

            for (int y = y1 - 1; y >= y0; y--) {
                for (int xi = 0; xi < n_cols; xi++) {
                    int x = shuffle_x_[xi];
                    int idx = y * w + x;
                    if (processed_[idx]) continue;

                    Cell cell = terrain.GetCell(x, y);
                    if (state_lut_[cell.material_id] != MaterialState::Liquid ||
                        !gravity_lut_[cell.material_id]) continue;

                    int cell_density = density_lut_[cell.material_id];
                    int flow         = flow_lut_[cell.material_id];
                    float drag       = liquid_drag_lut_[cell.material_id];

                    // Gravity accumulation — only vertical, no vel_x for liquids
                    vel_y_[idx] = std::min(vel_y_[idx] + GRAVITY, TERMINAL_VY);

                    // Convert vertical velocity to integer step
                    int dy = static_cast<int>(vel_y_[idx]);
                    vel_y_[idx] -= static_cast<float>(dy);

                    // What this liquid can displace
                    auto can_displace = [&](const Terrain& t, int nx, int ny) -> bool {
                        Cell nc = t.GetCell(nx, ny);
                        MaterialState ns = state_lut_[nc.material_id];
                        return ns == MaterialState::None
                            || ns == MaterialState::Gas
                            || (ns == MaterialState::Liquid &&
                                cell_density > density_lut_[nc.material_id]);
                    };

                    // ── Vertical phase: trace path downward ──────────────────
                    if (dy > 0) {
                        TraceResult tr = TracePath(terrain, x, y, x, y + dy, w, h, can_displace);
                        if (tr.moved) {
                            int didx = tr.y * w + tr.x;
                            SwapCells(terrain, x, y, tr.x, tr.y, w);
                            processed_[didx] = 1;
                            // If we hit a floor (didn't reach full dy), absorb velocity
                            if (tr.y != y + dy) {
                                vel_y_[didx] = 0.0f;
                            }
                            continue;
                        }

                        // Blocked straight down — try diagonal-down
                        // Preferred direction alternates per column to avoid directional bias
                        int pref = (x & 1) ? 1 : -1;
                        bool moved_diag = false;
                        for (int pass = 0; pass < 2 && !moved_diag; pass++) {
                            int d = (pass == 0) ? pref : -pref;
                            int nx = x + d;
                            if (nx < 0 || nx >= w) continue;

                            Cell diag = terrain.GetCell(nx, y + 1);
                            Cell side = terrain.GetCell(nx, y);
                            MaterialState diag_st = state_lut_[diag.material_id];
                            MaterialState side_st = state_lut_[side.material_id];

                            bool diag_ok = (diag_st == MaterialState::None ||
                                           diag_st == MaterialState::Gas ||
                                           (diag_st == MaterialState::Liquid &&
                                            cell_density > density_lut_[diag.material_id]));
                            bool side_ok = (side_st == MaterialState::None ||
                                           side_st == MaterialState::Gas ||
                                           side_st == MaterialState::Liquid);

                            if (diag_ok && side_ok) {
                                int didx = (y + 1) * w + nx;
                                if (!processed_[didx]) {
                                    SwapCells(terrain, x, y, nx, y + 1, w);
                                    processed_[didx] = 1;
                                    vel_y_[didx] = 0.0f;
                                    moved_diag = true;
                                }
                            }
                        }
                        if (moved_diag) continue;

                        // Completely blocked vertically — absorb vertical velocity
                        vel_y_[idx] = 0.0f;
                    }

                    // ── Horizontal phase: spread 1 cell per tick ─────────────
                    // Only runs when the liquid is grounded — i.e. the cell directly
                    // below is something it cannot fall into. This mirrors the
                    // FallingSandJava isFreeFalling flag: while in mid-air the liquid
                    // accumulates downward velocity and must NOT spread sideways.
                    if (flow <= 0) continue;

                    // Density-based floating: if the cell below is a heavier liquid,
                    // swap upward so lighter liquids rise to the surface.
                    if (y + 1 < h) {
                        Cell below = terrain.GetCell(x, y + 1);
                        if (state_lut_[below.material_id] == MaterialState::Liquid &&
                            density_lut_[below.material_id] > cell_density &&
                            !processed_[(y + 1) * w + x]) {
                            SwapCells(terrain, x, y, x, y + 1, w);
                            processed_[(y + 1) * w + x] = 1;
                            continue;
                        }
                    }

                    if (y + 1 < h) {
                        Cell below = terrain.GetCell(x, y + 1);
                        MaterialState below_st = state_lut_[below.material_id];
                        bool can_fall_below =
                            below_st == MaterialState::None ||
                            below_st == MaterialState::Gas  ||
                            (below_st == MaterialState::Liquid &&
                             cell_density > density_lut_[below.material_id]);
                        if (can_fall_below) continue;  // free-falling — don't spread
                    }

                    // Viscous drag: high-drag liquids probabilistically skip horizontal flow.
                    if (drag > 0.0f) {
                        uint32_t r = Xorshift32();
                        float roll = (r & 0xFFFF) / 65535.0f;
                        if (roll < drag) continue;  // viscous: skip horizontal step
                    }

                    // Direction preference: alternates per-cell-position to avoid bias.
                    // Using column parity (not tick_counter) so it doesn't oscillate
                    // back-and-forth on consecutive ticks.
                    int pref = (x & 1) ? 1 : -1;

                    for (int pass = 0; pass < 2; pass++) {
                        int d = (pass == 0) ? pref : -pref;

                        // Walk up to flow_rate cells in direction d, stopping at first obstacle
                        for (int step = 0; step < flow; step++) {
                            int nx = x + d * (step + 1);
                            if (nx < 0 || nx >= w) break;

                            Cell nc = terrain.GetCell(nx, y);
                            MaterialState ns = state_lut_[nc.material_id];
                            if (ns != MaterialState::None && ns != MaterialState::Gas) break;

                            // Check the cell one further to see if flow can continue,
                            // or if this is the last reachable slot — either way, occupy it
                            int nidx = y * w + nx;
                            if (processed_[nidx]) break;

                            SwapCells(terrain, x, y, nx, y, w);
                            processed_[nidx] = 1;
                            goto next_cell;  // moved — done for this cell
                        }
                    }
                    next_cell:;
                }
            }
        }
    }
}

// ── Gas Simulation ────────────────────────────────────────────────────────────

void TerrainSimulator::SimulateGas(Terrain& terrain) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    // Top-to-bottom scan (gas rises)
    for (int cy = 0; cy < chunks_y_; cy++) {
        for (int cx = 0; cx < chunks_x_; cx++) {
            if (!chunk_active_[ChunkIndex(cx, cy)]) continue;

            int x0 = cx * SIM_CHUNK;
            int y0 = cy * SIM_CHUNK;
            int x1 = std::min(x0 + SIM_CHUNK, w);
            int y1 = std::min(y0 + SIM_CHUNK, h);

            ShuffleColumns(shuffle_x_, x0, x1, rng_state_);
            int n_cols = x1 - x0;

            for (int y = y0; y < y1; y++) {
                for (int xi = 0; xi < n_cols; xi++) {
                    int x = shuffle_x_[xi];
                    int idx = y * w + x;
                    if (processed_[idx]) continue;

                    Cell cell = terrain.GetCell(x, y);
                    if (state_lut_[cell.material_id] != MaterialState::Gas) continue;

                    int cell_density = density_lut_[cell.material_id];
                    int cell_rise    = rise_rate_lut_[cell.material_id];
                    int cell_disp    = dispersion_lut_[cell.material_id];

                    // Lifetime / dissipation
                    int lt = lifetime_lut_[cell.material_id];
                    if (lt > 0) {
                        if (mass_[idx] <= 1) {
                            // Dissipate into air
                            Cell air = {0, cell.background_id};
                            terrain.SetCell(x, y, air);
                            mass_[idx] = 0;
                            ZeroVelocity(idx);
                            processed_[idx] = 1;
                            MarkDirty(x, y);
                            continue;
                        }
                        mass_[idx]--;
                    }

                    // Anti-gravity accumulation
                    if (cell_rise > 0) {
                        vel_y_[idx] = std::max(vel_y_[idx] - GAS_ANTIGRAVITY, GAS_TERMINAL_VY);
                    }

                    // Random horizontal perturbation for natural dispersion
                    uint32_t r = Xorshift32();
                    vel_x_[idx] += static_cast<float>(static_cast<int>(r % 3) - 1) * 0.2f;
                    // Clamp horizontal
                    vel_x_[idx] = std::max(-TERMINAL_VX, std::min(TERMINAL_VX, vel_x_[idx]));

                    int dy = static_cast<int>(vel_y_[idx]);
                    vel_y_[idx] -= static_cast<float>(dy);
                    int dx = static_cast<int>(vel_x_[idx]);
                    vel_x_[idx] -= static_cast<float>(dx);

                    // Gas only displaces air (None state)
                    auto can_displace = [&](const Terrain& t, int nx, int ny) -> bool {
                        return state_lut_[t.GetCell(nx, ny).material_id] == MaterialState::None;
                    };

                    bool rose = false;

                    if (cell_rise > 0 && (dy != 0 || dx != 0)) {
                        TraceResult tr = TracePath(terrain, x, y, x + dx, y + dy, w, h, can_displace);
                        if (tr.moved) {
                            int didx = tr.y * w + tr.x;
                            SwapCells(terrain, x, y, tr.x, tr.y, w);
                            processed_[didx] = 1;
                            bool blocked = (tr.y != y + dy);
                            if (blocked) {
                                vel_y_[didx] = 0.0f;
                            }
                            rose = true;
                        } else if (dy < 0 && y > 0) {
                            // Blocked rising — try diagonal-up
                            int pref = ((x + y) & 1) ? 1 : -1;
                            for (int pass = 0; pass < 2 && !rose; pass++) {
                                int d = (pass == 0) ? pref : -pref;
                                int nx = x + d;
                                if (nx < 0 || nx >= w || y - 1 < 0) continue;

                                Cell diag = terrain.GetCell(nx, y - 1);
                                Cell side = terrain.GetCell(nx, y);
                                bool diag_ok = state_lut_[diag.material_id] == MaterialState::None;
                                bool side_ok = (state_lut_[side.material_id] == MaterialState::None ||
                                               state_lut_[side.material_id] == MaterialState::Gas);

                                if (diag_ok && side_ok) {
                                    int didx = (y - 1) * w + nx;
                                    if (!processed_[didx]) {
                                        SwapCells(terrain, x, y, nx, y - 1, w);
                                        processed_[didx] = 1;
                                        vel_y_[didx] = 0.0f;
                                        rose = true;
                                    }
                                }
                            }
                        }
                    }

                    if (rose) continue;

                    // Horizontal dispersion
                    if (cell_disp <= 0) continue;

                    int pref = ((x + y) & 1) ? 1 : -1;
                    for (int pass = 0; pass < 2; pass++) {
                        int d = (pass == 0) ? pref : -pref;
                        TraceResult tr = TracePath(terrain, x, y, x + d * cell_disp, y, w, h, can_displace);
                        if (tr.moved) {
                            int didx = tr.y * w + tr.x;
                            SwapCells(terrain, x, y, tr.x, tr.y, w);
                            processed_[didx] = 1;
                            break;
                        }
                    }
                }
            }
        }
    }
}

// ── Chunk Prune ───────────────────────────────────────────────────────────────

void TerrainSimulator::PruneInactiveChunks(const Terrain& terrain) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    for (int cy = 0; cy < chunks_y_; cy++) {
        for (int cx = 0; cx < chunks_x_; cx++) {
            int idx = ChunkIndex(cx, cy);
            if (!chunk_active_[idx]) continue;

            int x0 = cx * SIM_CHUNK;
            int y0 = cy * SIM_CHUNK;
            int x1 = std::min(x0 + SIM_CHUNK, w);
            int y1 = std::min(y0 + SIM_CHUNK, h);

            bool has_mobile = false;
            for (int y = y0; y < y1 && !has_mobile; y++) {
                for (int x = x0; x < x1 && !has_mobile; x++) {
                    Cell cell = terrain.GetCell(x, y);
                    MaterialState st = state_lut_[cell.material_id];
                    if (st == MaterialState::Gas) {
                        has_mobile = true;
                    } else if (st == MaterialState::Powder && gravity_lut_[cell.material_id]) {
                        has_mobile = true;
                    } else if (st == MaterialState::Liquid && gravity_lut_[cell.material_id]) {
                        has_mobile = true;
                    }
                }
            }
            if (!has_mobile) {
                chunk_active_[idx] = false;
            }
        }
    }
}

// ── Cell Conversion Helper ────────────────────────────────────────────────────

void TerrainSimulator::ConvertCell(Terrain& terrain, int x, int y, int w, int new_mat_id) {
    int idx = y * w + x;
    Cell old_cell = terrain.GetCell(x, y);

    // Build new cell, preserving background
    Cell new_cell;
    new_cell.material_id  = static_cast<MaterialID>(new_mat_id);
    new_cell.background_id = old_cell.background_id;
    terrain.SetCell(x, y, new_cell);

    // Clear reactive state — velocity stays (gives momentum to e.g. Steam)
    ignited_[idx]    = 0;
    sleeping_[idx]   = 0;
    stationary_[idx] = 0;
    crack_[idx]      = 0;

    // Initialize health for the new material
    if (new_mat_id >= 0 && new_mat_id < static_cast<int>(max_health_lut_.size())) {
        int mh = max_health_lut_[new_mat_id];
        health_[idx] = static_cast<int16_t>(mh > 0 ? mh : 0);
    }

    // Gas lifetime
    if (new_mat_id >= 0 && new_mat_id < static_cast<int>(lifetime_lut_.size())) {
        if (lifetime_lut_[new_mat_id] > 0)
            mass_[idx] = static_cast<uint8_t>(std::min(lifetime_lut_[new_mat_id], 255));
    }

    MarkDirty(x, y);
}

void TerrainSimulator::ApplyStain(int idx, uint8_t r, uint8_t g, uint8_t b, float strength) {
    if (strength <= 0.0f || idx < 0) return;
    // Blend new stain onto existing stain
    float a = static_cast<float>(stain_a_[idx]) / 255.0f;
    float new_a = std::min(1.0f, a + strength);
    float t = (new_a > 0) ? strength / new_a : 0.0f;
    stain_r_[idx] = static_cast<uint8_t>(stain_r_[idx] * (1.f - t) + r * t);
    stain_g_[idx] = static_cast<uint8_t>(stain_g_[idx] * (1.f - t) + g * t);
    stain_b_[idx] = static_cast<uint8_t>(stain_b_[idx] * (1.f - t) + b * t);
    stain_a_[idx] = static_cast<uint8_t>(new_a * 255.0f);
}

// ── Apply Pending Mutations ───────────────────────────────────────────────────

void TerrainSimulator::ApplyPendingMutations(Terrain& terrain) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    for (const auto& m : pending_) {
        if (m.x < 0 || m.x >= w || m.y < 0 || m.y >= h) continue;
        int idx = m.y * w + m.x;

        switch (m.type) {
        case PendingMutation::Type::Convert:
            if (m.material_id < 0) {
                // Convert to Air
                Cell air = { 0, terrain.GetCell(m.x, m.y).background_id };
                terrain.SetCell(m.x, m.y, air);
                ZeroVelocity(idx);
                health_[idx] = 0;
                ignited_[idx] = 0;
                mass_[idx] = 0;
                MarkDirty(m.x, m.y);
            } else {
                ConvertCell(terrain, m.x, m.y, w, m.material_id);
            }
            break;
        case PendingMutation::Type::Damage:
            if (max_health_lut_[terrain.GetCell(m.x,m.y).material_id] > 0) {
                health_[idx] -= static_cast<int16_t>(m.amount);
            }
            break;
        case PendingMutation::Type::TempChange:
            temp_[idx] += m.delta;
            break;
        case PendingMutation::Type::Ignite:
            ignited_[idx] = 1;
            MarkDirty(m.x, m.y);
            break;
        case PendingMutation::Type::Extinguish:
            ignited_[idx] = 0;
            MarkDirty(m.x, m.y);
            break;
        }
    }
    pending_.clear();
}

// ── Temperature Simulation ────────────────────────────────────────────────────

void TerrainSimulator::SimulateTemperature(Terrain& terrain) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    static const int DX[] = {1, -1, 0,  0};
    static const int DY[] = {0,  0, 1, -1};

    for (int cy = 0; cy < chunks_y_; cy++) {
        for (int cx = 0; cx < chunks_x_; cx++) {
            if (!chunk_active_[ChunkIndex(cx, cy)]) continue;

            int x0 = cx * SIM_CHUNK, y0 = cy * SIM_CHUNK;
            int x1 = std::min(x0 + SIM_CHUNK, w);
            int y1 = std::min(y0 + SIM_CHUNK, h);

            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    int idx = y * w + x;
                    Cell cell = terrain.GetCell(x, y);
                    int mat = cell.material_id;

                    // Heat conduction to 4 neighbors (suppressed if source material
                    // does not conduct heat, e.g. Air — incoming heat from neighbors
                    // and ambient/combustion still apply).
                    float cond = heat_conductivity_lut_[mat];
                    if (cond > 0.0f && conducts_heat_lut_[mat]) {
                        for (int d = 0; d < 4; d++) {
                            int nx = x + DX[d], ny = y + DY[d];
                            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                            int nidx = ny * w + nx;
                            float dt_val = cond * (temp_[idx] - temp_[nidx]);
                            if (std::abs(dt_val) < MIN_CONDUCT_DT) continue;
                            temp_[idx]  -= dt_val;
                            temp_[nidx] += dt_val;

                            // Fire on_heat callback for target cell when delta >= 1.0°
                            if (std::abs(dt_val) >= 1.0f) {
                                Cell nc = terrain.GetCell(nx, ny);
                                int nmat = nc.material_id;
                                if (nmat >= 0 && nmat < 256 && on_heat_cbs_[nmat]) {
                                    SimCell sc;
                                    sc.terrain = &terrain;
                                    sc.x = nx; sc.y = ny; sc.w = w; sc.h = h;
                                    sc.temp = temp_.data(); sc.health = health_.data();
                                    sc.ignited = ignited_.data(); sc.pending = &pending_;
                                    sc.registry = &registry_;
                                    on_heat_cbs_[nmat](sc, dt_val);
                                }
                            }
                        }
                    }

                    // Ambient pull: material slowly returns to its equilibrium temperature
                    float amb = ambient_temp_lut_[mat];
                    temp_[idx] += (amb - temp_[idx]) * AMBIENT_PULL;

                    // Combustion: burning cells heat neighbors
                    if (ignited_[idx]) {
                        float heat = combustion_heat_lut_[mat];
                        if (heat > 0.0f) {
                            for (int d = 0; d < 4; d++) {
                                int nx = x + DX[d], ny = y + DY[d];
                                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                                temp_[ny * w + nx] += heat;
                            }
                        }
                        // Burning stains the cell toward ember-dark
                        ApplyStain(idx, 40, 20, 10, 0.002f);
                    }

                    // Ignition check
                    float ign = ignition_temp_lut_[mat];
                    if (!ignited_[idx] && ign >= 0.0f && temp_[idx] >= ign) {
                        ignited_[idx] = 1;
                        MarkDirty(x, y);
                    }

                    // Phase change — above-threshold rules
                    for (const auto& [thr, tid] : phase_above_lut_[mat]) {
                        if (temp_[idx] >= thr) {
                            pending_.push_back({PendingMutation::Type::Convert, x, y, tid});
                            goto next_temp_cell;
                        }
                    }
                    // Phase change — below-threshold rules
                    for (const auto& [thr, tid] : phase_below_lut_[mat]) {
                        if (temp_[idx] <= thr) {
                            pending_.push_back({PendingMutation::Type::Convert, x, y, tid});
                            goto next_temp_cell;
                        }
                    }
                    next_temp_cell:;
                }
            }
        }
    }
}

// ── Health Simulation ─────────────────────────────────────────────────────────

void TerrainSimulator::SimulateHealth(Terrain& terrain) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    static const int DX[] = {1, -1, 0,  0};
    static const int DY[] = {0,  0, 1, -1};

    for (int cy = 0; cy < chunks_y_; cy++) {
        for (int cx = 0; cx < chunks_x_; cx++) {
            if (!chunk_active_[ChunkIndex(cx, cy)]) continue;

            int x0 = cx * SIM_CHUNK, y0 = cy * SIM_CHUNK;
            int x1 = std::min(x0 + SIM_CHUNK, w);
            int y1 = std::min(y0 + SIM_CHUNK, h);

            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    int idx = y * w + x;
                    Cell cell = terrain.GetCell(x, y);
                    int mat = cell.material_id;

                    // Initialize health on first encounter (health==0, max_health>0)
                    if (max_health_lut_[mat] > 0 && health_[idx] == 0) {
                        health_[idx] = static_cast<int16_t>(max_health_lut_[mat]);
                    }

                    // Fire damage
                    if (ignited_[idx] && max_health_lut_[mat] > 0) {
                        health_[idx] -= 2;
                    }

                    // Corrosion: damage adjacent cells
                    int cdmg = corrode_damage_lut_[mat];
                    if (cdmg > 0) {
                        for (int d = 0; d < 4; d++) {
                            int nx = x + DX[d], ny = y + DY[d];
                            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                            int nidx = ny * w + nx;
                            Cell nc = terrain.GetCell(nx, ny);
                            if (nc.material_id == cell.material_id) continue; // no self-corrosion
                            if (max_health_lut_[nc.material_id] > 0) {
                                pending_.push_back({PendingMutation::Type::Damage, nx, ny,
                                                   -1, cdmg, 0.0f});
                                // Stain neighbor with acid color
                                ApplyStain(nidx, stain_r_lut_[mat], stain_g_lut_[mat],
                                           stain_b_lut_[mat], stain_strength_lut_[mat]);
                            }
                        }
                        // Self-consume
                        if (corrode_self_lut_[mat] && max_health_lut_[mat] > 0)
                            health_[idx] -= cdmg;
                        else if (corrode_self_lut_[mat])
                            health_[idx]--;  // acid drains even if indestructible self
                    }

                    // Death check
                    if (max_health_lut_[mat] > 0 && health_[idx] <= 0) {
                        int dp = death_product_lut_[mat];
                        pending_.push_back({PendingMutation::Type::Convert, x, y, dp});
                    }
                }
            }
        }
    }
}

// ── Special Simulation ────────────────────────────────────────────────────────

void TerrainSimulator::SimulateSpecial(Terrain& terrain) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    for (int cy = 0; cy < chunks_y_; cy++) {
        for (int cx = 0; cx < chunks_x_; cx++) {
            if (!chunk_active_[ChunkIndex(cx, cy)]) continue;

            int x0 = cx * SIM_CHUNK, y0 = cy * SIM_CHUNK;
            int x1 = std::min(x0 + SIM_CHUNK, w);
            int y1 = std::min(y0 + SIM_CHUNK, h);

            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    int idx = y * w + x;
                    Cell cell = terrain.GetCell(x, y);
                    int mat = cell.material_id;

                    // Stain fade
                    if (stain_a_[idx] > 0) {
                        float fade = stain_fade_lut_[mat] * 255.0f;
                        if (fade < 1.0f) fade = 1.0f;  // ensure at least 1/255 per tick
                        int new_a = static_cast<int>(stain_a_[idx]) - static_cast<int>(fade);
                        stain_a_[idx] = static_cast<uint8_t>(std::max(0, new_a));
                    }

                    // Solidification over time
                    int sticks = solidify_ticks_lut_[mat];
                    if (sticks > 0) {
                        if (processed_[idx]) {
                            stationary_[idx] = 0;  // moved this tick
                        } else {
                            stationary_[idx]++;
                            if (stationary_[idx] >= sticks) {
                                int si = solidify_into_lut_[mat];
                                if (si >= 0) {
                                    pending_.push_back({PendingMutation::Type::Convert,
                                                        x, y, si});
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

// ── Explosion System ──────────────────────────────────────────────────────────

void TerrainSimulator::TriggerExplosion(Terrain& terrain, int cx, int cy, int radius, int strength) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    // Generate square-perimeter points — approximates a circle
    for (int px = -radius; px <= radius; px++) {
        for (int py = -radius; py <= radius; py++) {
            if (std::abs(px) != radius && std::abs(py) != radius) continue;

            // Vary effective radius ±1 per ray for organic shape
            int eff_r = radius + static_cast<int>(Xorshift32() % 3) - 1;
            if (eff_r < 1) eff_r = 1;

            // Step from center toward perimeter point cell-by-cell (Bresenham)
            int dx = px, dy = py;
            int sx = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
            int sy = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;
            int adx = std::abs(dx), ady = std::abs(dy);
            int err = adx - ady;
            int rx = cx, ry = cy;
            int steps = 0;

            while (steps < eff_r) {
                int e2 = 2 * err;
                int nx = rx, ny = ry;
                if (e2 > -ady) { err -= ady; nx += sx; }
                if (e2 <  adx) { err += adx; ny += sy; }
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) break;

                int idx = ny * w + nx;
                Cell cell = terrain.GetCell(nx, ny);
                int mat = cell.material_id;
                float dist_f = static_cast<float>(steps) / static_cast<float>(eff_r);
                int damage = static_cast<int>(strength * (1.0f - dist_f));

                // Stop if blast_resistance exceeds remaining strength
                if (blast_resistance_lut_[mat] > damage && state_lut_[mat] != MaterialState::None) {
                    // Apply crack damage to resistant solids
                    if (state_lut_[mat] == MaterialState::Solid && damage > 0) {
                        int new_crack = crack_[idx] + damage;
                        crack_[idx] = static_cast<uint8_t>(std::min(new_crack, 255));
                        ApplyStain(idx, 30, 20, 10, 0.15f);
                    }
                    break;
                }

                // Apply effects to cell
                if (state_lut_[mat] != MaterialState::None) {
                    // Damage
                    if (max_health_lut_[mat] > 0) {
                        pending_.push_back({PendingMutation::Type::Damage, nx, ny,
                                           -1, damage, 0.0f});
                    } else if (state_lut_[mat] == MaterialState::Powder ||
                               state_lut_[mat] == MaterialState::Liquid ||
                               state_lut_[mat] == MaterialState::Gas) {
                        // Displaceable cells — convert to air and give velocity kick
                        Cell air = {0, cell.background_id};
                        terrain.SetCell(nx, ny, air);
                        ZeroVelocity(idx);
                        float vkick = static_cast<float>(damage) * 0.05f;
                        vel_x_[idx] = static_cast<float>(sx) * vkick;
                        vel_y_[idx] = static_cast<float>(sy) * vkick - 1.0f;
                        MarkDirty(nx, ny);
                    }
                    // Heat from explosion
                    temp_[idx] += static_cast<float>(strength) * 8.0f * (1.0f - dist_f);
                    // Char stain
                    ApplyStain(idx, 30, 20, 10, 0.3f * (1.0f - dist_f));
                }

                // 50% chance to spawn ExplosionSpark in empty cells near the blast
                if (state_lut_[mat] == MaterialState::None) {
                    if ((Xorshift32() & 1) == 0) {
                        auto* spark = registry_.GetMaterial("base:ExplosionSpark");
                        if (spark) {
                            Cell sc; sc.material_id = spark->runtime_id;
                            sc.background_id = cell.background_id;
                            terrain.SetCell(nx, ny, sc);
                            if (lifetime_lut_[spark->runtime_id] > 0)
                                mass_[idx] = static_cast<uint8_t>(
                                    std::min(lifetime_lut_[spark->runtime_id], 255));
                            vel_y_[idx] = -2.0f + static_cast<float>(Xorshift32() % 3) - 1.0f;
                            vel_x_[idx] = static_cast<float>(static_cast<int>(Xorshift32() % 5)) - 2.0f;
                            MarkDirty(nx, ny);
                        }
                    }
                }

                rx = nx; ry = ny;
                steps++;
            }
        }
    }

    // Activate chunks in explosion radius
    NotifyModified(cx - radius, cy - radius, radius * 2 + 1, radius * 2 + 1);
}

// ── Lua Callbacks ─────────────────────────────────────────────────────────────

void TerrainSimulator::DispatchLuaCallbacks(Terrain& terrain) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    for (int cy = 0; cy < chunks_y_; cy++) {
        for (int cx = 0; cx < chunks_x_; cx++) {
            if (!chunk_active_[ChunkIndex(cx, cy)]) continue;

            int x0 = cx * SIM_CHUNK, y0 = cy * SIM_CHUNK;
            int x1 = std::min(x0 + SIM_CHUNK, w);
            int y1 = std::min(y0 + SIM_CHUNK, h);

            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    Cell cell = terrain.GetCell(x, y);
                    int mat = cell.material_id;
                    if (mat < 0 || mat >= 256 || !on_tick_cbs_[mat]) continue;

                    SimCell sc;
                    sc.terrain  = &terrain;
                    sc.x = x; sc.y = y;
                    sc.w = w; sc.h = h;
                    sc.temp     = temp_.data();
                    sc.health   = health_.data();
                    sc.ignited  = ignited_.data();
                    sc.pending  = &pending_;
                    sc.registry = &registry_;

                    on_tick_cbs_[mat](sc);
                }
            }
        }
    }
    // Mutations queued during callbacks are flushed by the subsequent ApplyPendingMutations call
}

// ── Crack / Fragment Damage ───────────────────────────────────────────────────

void TerrainSimulator::ApplyCrackDamage(Terrain& terrain, int x, int y, int damage) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();
    if (x < 0 || x >= w || y < 0 || y >= h) return;

    int idx = y * w + x;
    Cell cell = terrain.GetCell(x, y);
    if (state_lut_[cell.material_id] != MaterialState::Solid) return;

    // Accumulate crack intensity
    int new_crack = static_cast<int>(crack_[idx]) + damage;
    crack_[idx] = static_cast<uint8_t>(std::min(new_crack, 255));
    ApplyStain(idx, 50, 40, 30, std::min(1.0f, damage * 0.01f));
    MarkDirty(x, y);

    // Propagate cracks radially using Bresenham rays
    int mat = cell.material_id;
    int n_rays = (mat < static_cast<int>(state_lut_.size()) &&
                  // fragmentation style Random → more rays
                  false) ? 16 : 8;
    // TODO: vary ray count by FragStyle (Grid=4, Random=8-16)

    for (int r = 0; r < n_rays; r++) {
        // Distribute rays evenly
        float angle = static_cast<float>(r) / static_cast<float>(n_rays) * 6.283185f;
        float len = static_cast<float>(damage) * 0.5f;
        int tx = x + static_cast<int>(std::cos(angle) * len);
        int ty = y + static_cast<int>(std::sin(angle) * len);

        // Walk toward target
        int dx2 = tx - x, dy2 = ty - y;
        int sx = (dx2 > 0) ? 1 : (dx2 < 0) ? -1 : 0;
        int sy2 = (dy2 > 0) ? 1 : (dy2 < 0) ? -1 : 0;
        int adx = std::abs(dx2), ady = std::abs(dy2);
        int err2 = adx - ady;
        int rx = x, ry = y;

        for (int step = 0; step < static_cast<int>(len); step++) {
            int e2 = 2 * err2;
            int nx = rx, ny = ry;
            if (e2 > -ady) { err2 -= ady; nx += sx; }
            if (e2 <  adx) { err2 += adx; ny += sy2; }
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) break;
            int nidx = ny * w + nx;
            Cell nc = terrain.GetCell(nx, ny);
            if (state_lut_[nc.material_id] != MaterialState::Solid) break;
            int nc2 = static_cast<int>(crack_[nidx]) + damage / 2;
            crack_[nidx] = static_cast<uint8_t>(std::min(nc2, 255));
            ApplyStain(nidx, 50, 40, 30, std::min(1.0f, (damage / 2) * 0.01f));
            MarkDirty(nx, ny);
            rx = nx; ry = ny;
        }
    }
}

// ── SpawnParticleAt ──────────────────────────────────────────────────────────

void TerrainSimulator::SpawnParticleAt(int x, int y, int w, float vx, float vy, int ttl) {
    int idx = y * w + x;
    if (idx < 0 || idx >= static_cast<int>(vel_x_.size())) return;

    vel_x_[idx] = vx;
    vel_y_[idx] = vy;
    sleeping_[idx] = 0;

    // Override lifetime if requested (only meaningful for gas/particle materials)
    if (ttl > 0) {
        mass_[idx] = static_cast<uint8_t>(std::min(ttl, 255));
    }

    MarkDirty(x, y);
}
