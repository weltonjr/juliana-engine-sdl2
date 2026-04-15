#include "terrain/TerrainSimulator.h"
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
    state_lut_.resize(256, MaterialState::None);
    gravity_lut_.resize(256, false);
    flow_lut_.resize(256, 0);
    density_lut_.resize(256, 0);
    rise_rate_lut_.resize(256, 0);
    dispersion_lut_.resize(256, 0);
    lifetime_lut_.resize(256, 0);
    friction_lut_.resize(256, 0.8f);
    liquid_drag_lut_.resize(256, 0.85f);

    for (int i = 0; i < 256; i++) {
        auto* mat = registry.GetMaterialByRuntimeID(static_cast<MaterialID>(i));
        if (mat) {
            state_lut_[i]       = mat->state;
            gravity_lut_[i]     = mat->gravity;
            flow_lut_[i]        = mat->flow_rate;
            density_lut_[i]     = mat->density;
            rise_rate_lut_[i]   = mat->rise_rate;
            dispersion_lut_[i]  = mat->dispersion;
            lifetime_lut_[i]    = mat->lifetime;
            friction_lut_[i]    = mat->friction;
            liquid_drag_lut_[i] = mat->liquid_drag;
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
    std::swap(mass_[ia],  mass_[ib]);
    std::swap(vel_x_[ia], vel_x_[ib]);
    std::swap(vel_y_[ia], vel_y_[ib]);

    MarkDirty(ax, ay);
    MarkDirty(bx, by);
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

int TerrainSimulator::EffectivePressure(const Terrain& terrain, int x, int y, int w) const {
    int idx = y * w + x;
    int pressure = mass_[idx];
    for (int dy = 1; dy <= MAX_COLUMN_SCAN; dy++) {
        int ay = y - dy;
        if (ay < 0) break;
        Cell above = terrain.GetCell(x, ay);
        if (state_lut_[above.material_id] != MaterialState::Liquid) break;
        pressure += COLUMN_WEIGHT;
    }
    return pressure;
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

    // Clear processed flags each tick
    std::memset(processed_.data(), 0, processed_.size());

    SimulatePowder(terrain);
    SimulateLiquid(terrain);
    SimulateGas(terrain);
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
                        // Blocked going straight — try diagonal
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
                            // Fully blocked — settle
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
                    // Runs when the cell didn't fall this tick (dy==0 or fully blocked).
                    // Tries each direction up to flow_rate steps, moving 1 cell at a time.
                    // This matches FallingSandJava dispersionRate behaviour: the cell
                    // walks sideways into the first available air/gas slot.
                    if (flow <= 0) continue;

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

// ── Pressure Equalization ─────────────────────────────────────────────────────

void TerrainSimulator::EqualizePressure(Terrain& terrain) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    for (int cy = chunks_y_ - 1; cy >= 0; cy--) {
        for (int cx = 0; cx < chunks_x_; cx++) {
            if (!chunk_active_[ChunkIndex(cx, cy)]) continue;

            int x0 = cx * SIM_CHUNK;
            int y0 = cy * SIM_CHUNK;
            int x1 = std::min(x0 + SIM_CHUNK, w);
            int y1 = std::min(y0 + SIM_CHUNK, h);

            for (int y = y1 - 1; y >= y0; y--) {
                for (int x = x0; x < x1; x++) {
                    int idx = y * w + x;
                    Cell cell = terrain.GetCell(x, y);
                    if (state_lut_[cell.material_id] != MaterialState::Liquid) continue;

                    int p_self = EffectivePressure(terrain, x, y, w);

                    int dirs[2] = {-1, 1};
                    if ((x + y) & 1) { dirs[0] = 1; dirs[1] = -1; }

                    for (int d : dirs) {
                        int nx = x + d;
                        if (nx < 0 || nx >= w) continue;
                        int nidx = y * w + nx;

                        Cell neighbor = terrain.GetCell(nx, y);
                        MaterialState n_st = state_lut_[neighbor.material_id];

                        if (n_st == MaterialState::None || n_st == MaterialState::Gas) {
                            if (p_self > SPAWN_THRESHOLD && mass_[idx] > MIN_TRANSFER) {
                                uint8_t transfer = static_cast<uint8_t>(std::min(
                                    static_cast<int>(mass_[idx]) / 4,
                                    static_cast<int>(mass_[idx]) - 1));
                                if (transfer < MIN_TRANSFER) continue;

                                terrain.SetCell(nx, y, cell);
                                mass_[nidx] = transfer;
                                mass_[idx] -= transfer;
                                // New liquid cell starts with no velocity
                                ZeroVelocity(nidx);

                                if (mass_[idx] == 0) {
                                    terrain.SetCell(x, y, {0, cell.background_id});
                                    ZeroVelocity(idx);
                                }

                                MarkDirty(x, y);
                                MarkDirty(nx, y);
                            }
                        } else if (n_st == MaterialState::Liquid &&
                                   neighbor.material_id == cell.material_id) {
                            int p_neighbor = EffectivePressure(terrain, nx, y, w);
                            int delta = (p_self - p_neighbor) / 4;
                            if (delta > MIN_TRANSFER) {
                                uint8_t transfer = static_cast<uint8_t>(std::min(delta, 255));
                                transfer = std::min(transfer, mass_[idx]);
                                int new_n = static_cast<int>(mass_[nidx]) + transfer;
                                if (new_n > MAX_MASS) {
                                    transfer = MAX_MASS - mass_[nidx];
                                }
                                if (transfer > 0) {
                                    mass_[idx]  -= transfer;
                                    mass_[nidx] += transfer;

                                    if (mass_[idx] == 0) {
                                        terrain.SetCell(x, y, {0, cell.background_id});
                                        ZeroVelocity(idx);
                                        MarkDirty(x, y);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
