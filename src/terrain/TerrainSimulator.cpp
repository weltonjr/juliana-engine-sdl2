#include "terrain/TerrainSimulator.h"
#include "package/MaterialDef.h"
#include <algorithm>
#include <cstring>

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

    max_flow_ = 1;
    max_rise_ = 1;

    for (int i = 0; i < 256; i++) {
        auto* mat = registry.GetMaterialByRuntimeID(static_cast<MaterialID>(i));
        if (mat) {
            state_lut_[i] = mat->state;
            gravity_lut_[i] = mat->gravity;
            flow_lut_[i] = mat->flow_rate;
            density_lut_[i] = mat->density;
            rise_rate_lut_[i] = mat->rise_rate;
            dispersion_lut_[i] = mat->dispersion;
            lifetime_lut_[i] = mat->lifetime;

            if (mat->flow_rate > max_flow_) max_flow_ = mat->flow_rate;
            if (mat->rise_rate > max_rise_) max_rise_ = mat->rise_rate;
        }
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

    // Mark the destination chunk and its neighbors as active for next tick
    int cx = x / SIM_CHUNK;
    int cy = y / SIM_CHUNK;
    if (cx >= 0 && cx < chunks_x_ && cy >= 0 && cy < chunks_y_) {
        chunk_active_[ChunkIndex(cx, cy)] = true;
        if (cy + 1 < chunks_y_) chunk_active_[ChunkIndex(cx, cy + 1)] = true;
        if (cy > 0) chunk_active_[ChunkIndex(cx, cy - 1)] = true;
        if (cx > 0) chunk_active_[ChunkIndex(cx - 1, cy)] = true;
        if (cx + 1 < chunks_x_) chunk_active_[ChunkIndex(cx + 1, cy)] = true;
    }
}

void TerrainSimulator::NotifyModified(int rx, int ry, int rw, int rh) {
    // If ScanActiveChunks hasn't run yet (fresh simulator), chunk arrays are
    // empty. A full scan on the first Update() will find everything anyway,
    // so we can safely bail here.
    if (chunks_x_ == 0 || chunks_y_ == 0 || chunk_active_.empty()) return;

    int cx0 = std::max(0, (rx - 1) / SIM_CHUNK);
    int cy0 = std::max(0, (ry - 1) / SIM_CHUNK);
    int cx1 = std::min(chunks_x_ - 1, (rx + rw) / SIM_CHUNK);
    int cy1 = std::min(chunks_y_ - 1, (ry + rh + SIM_CHUNK) / SIM_CHUNK);

    for (int cy = cy0; cy <= cy1; cy++) {
        for (int cx = cx0; cx <= cx1; cx++) {
            chunk_active_[ChunkIndex(cx, cy)] = true;
        }
    }
}

void TerrainSimulator::InitMassRegion(const Terrain& terrain, int rx, int ry, int rw, int rh) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();
    int total = w * h;
    // Fresh simulator or regenerated terrain with different dimensions —
    // overlays haven't been sized yet. ScanActiveChunks on the first Update()
    // will initialize everything, so skip here to avoid OOB writes.
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

    // Initialize overlays
    int total = w * h;
    processed_.resize(total, 0);
    mass_.resize(total, 0);

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
                         st == MaterialState::Gas) && (gravity_lut_[cell.material_id] ||
                         st == MaterialState::Gas)) {
                        active = true;
                    }
                }
            }
            chunk_active_[ChunkIndex(cx, cy)] = active;
        }
    }

    // Initialize mass for all liquid/gas cells
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

void TerrainSimulator::Update(Terrain& terrain) {
    any_dirty_ = false;
    dirty_rects_.clear();

    tick_counter_++;
    if (tick_counter_ < SIM_INTERVAL) return;
    tick_counter_ = 0;

    if (needs_full_scan_) {
        ScanActiveChunks(terrain);
        needs_full_scan_ = false;
    }

    int w = terrain.GetWidth();
    int h = terrain.GetHeight();
    int total = w * h;

    // Ensure overlays are sized correctly
    if (static_cast<int>(processed_.size()) != total) {
        processed_.resize(total, 0);
    }
    if (static_cast<int>(mass_.size()) != total) {
        mass_.resize(total, 0);
    }

    // Clear processed flags
    std::memset(processed_.data(), 0, processed_.size());

    // Simulate powder (bottom-to-top)
    SimulatePowder(terrain);

    // Simulate liquid (bottom-to-top, multi-pass for flow_rate)
    for (int pass = 0; pass < max_flow_; pass++) {
        SimulateLiquid(terrain, pass);
    }

    // Simulate gas (top-to-bottom, multi-pass for rise_rate)
    for (int pass = 0; pass < max_rise_; pass++) {
        SimulateGas(terrain, pass);
    }

    // Pressure equalization for liquids
    EqualizePressure(terrain);

    // Deactivate chunks with no movable cells left (one unified pass).
    PruneInactiveChunks(terrain);

    if (any_dirty_) {
        dirty_rects_.push_back({
            std::max(0, dirty_min_x_ - 1),
            std::max(0, dirty_min_y_ - 1),
            std::min(terrain.GetWidth(), dirty_max_x_ + 2) - std::max(0, dirty_min_x_ - 1),
            std::min(terrain.GetHeight(), dirty_max_y_ + 2) - std::max(0, dirty_min_y_ - 1)
        });
    }
}

// ── Powder Simulation ────────────────────────────────────────────────────────

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

            // Keep the chunk active as long as it contains ANY gravity
            // powder cell, whether or not it moved this tick. Deactivating
            // based on "did we swap" is too eager: processed_ flags, scan
            // order, and cross-chunk hand-offs can all produce a tick with
            // zero visible moves even though cells are still mid-air. The
            // extra cost of rescanning a settled pile (a few LUT lookups
            // per cell, once every SIM_INTERVAL ticks) is cheap compared to
            // losing animations.
            bool had_mobile = false;

            for (int y = y1 - 1; y >= y0; y--) {
                for (int x = x0; x < x1; x++) {
                    int idx = y * w + x;
                    if (processed_[idx]) continue;

                    Cell cell = terrain.GetCell(x, y);
                    if (state_lut_[cell.material_id] != MaterialState::Powder ||
                        !gravity_lut_[cell.material_id]) continue;

                    // Seen a powder cell — chunk is not empty of mobile content.
                    had_mobile = true;

                    int cell_density = density_lut_[cell.material_id];

                    // Try falling straight down
                    Cell below = terrain.GetCell(x, y + 1);
                    MaterialState below_st = state_lut_[below.material_id];

                    if (below_st == MaterialState::None || below_st == MaterialState::Gas ||
                        (below_st == MaterialState::Liquid && cell_density > density_lut_[below.material_id])) {
                        int didx = (y + 1) * w + x;
                        terrain.SetCell(x, y + 1, cell);
                        terrain.SetCell(x, y, below);
                        // Transfer mass (swap)
                        uint8_t tmp = mass_[idx];
                        mass_[idx] = mass_[didx];
                        mass_[didx] = tmp;
                        processed_[didx] = 1;
                        MarkDirty(x, y);
                        MarkDirty(x, y + 1);
                        had_mobile = true;
                        continue;
                    }

                    // Try diagonal
                    int dirs[2] = {-1, 1};
                    if ((x + y) & 1) { dirs[0] = 1; dirs[1] = -1; }

                    for (int d : dirs) {
                        int nx = x + d;
                        if (nx < 0 || nx >= w) continue;

                        Cell diag = terrain.GetCell(nx, y + 1);
                        MaterialState diag_st = state_lut_[diag.material_id];
                        Cell side = terrain.GetCell(nx, y);
                        MaterialState side_st = state_lut_[side.material_id];

                        bool diag_ok = (diag_st == MaterialState::None || diag_st == MaterialState::Gas ||
                                        (diag_st == MaterialState::Liquid && cell_density > density_lut_[diag.material_id]));
                        bool side_ok = (side_st == MaterialState::None || side_st == MaterialState::Gas ||
                                        side_st == MaterialState::Liquid);

                        if (diag_ok && side_ok) {
                            int didx = (y + 1) * w + nx;
                            terrain.SetCell(nx, y + 1, cell);
                            terrain.SetCell(x, y, diag);
                            uint8_t tmp = mass_[idx];
                            mass_[idx] = mass_[didx];
                            mass_[didx] = tmp;
                            processed_[didx] = 1;
                            MarkDirty(x, y);
                            MarkDirty(nx, y + 1);
                            break;
                        }
                    }
                }
            }

            // NOTE: deactivation is deferred to PruneInactiveChunks() after
            // all three simulators have run, because each sim only sees its
            // own material kind. A chunk holding only liquid would otherwise
            // be wrongly deactivated by SimulatePowder (or vice versa).
            (void)had_mobile;
        }
    }
}

// ── Liquid Simulation ────────────────────────────────────────────────────────

void TerrainSimulator::SimulateLiquid(Terrain& terrain, int pass) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    for (int cy = chunks_y_ - 1; cy >= 0; cy--) {
        for (int cx = 0; cx < chunks_x_; cx++) {
            if (!chunk_active_[ChunkIndex(cx, cy)]) continue;

            int x0 = cx * SIM_CHUNK;
            int y0 = cy * SIM_CHUNK;
            int x1 = std::min(x0 + SIM_CHUNK, w);
            int y1 = std::min(y0 + SIM_CHUNK, h - 1);

            // See powder comment — same rationale: any liquid cell present
            // in the chunk keeps it active, so we don't stall flow due to
            // scan-order / pass-order masking.
            bool had_mobile = false;

            for (int y = y1 - 1; y >= y0; y--) {
                for (int x = x0; x < x1; x++) {
                    int idx = y * w + x;
                    if (processed_[idx]) continue;

                    Cell cell = terrain.GetCell(x, y);
                    if (state_lut_[cell.material_id] != MaterialState::Liquid ||
                        !gravity_lut_[cell.material_id]) continue;

                    had_mobile = true;

                    int cell_density = density_lut_[cell.material_id];
                    int cell_flow = flow_lut_[cell.material_id];

                    // Pass 0: gravity fall + diagonal
                    // All passes: horizontal spread (if flow_rate > pass)
                    if (pass == 0) {
                        // Fall into air or gas
                        Cell below = terrain.GetCell(x, y + 1);
                        MaterialState below_st = state_lut_[below.material_id];

                        if (below_st == MaterialState::None || below_st == MaterialState::Gas) {
                            int didx = (y + 1) * w + x;
                            terrain.SetCell(x, y + 1, cell);
                            terrain.SetCell(x, y, below);
                            uint8_t tmp = mass_[idx];
                            mass_[idx] = mass_[didx];
                            mass_[didx] = tmp;
                            processed_[didx] = 1;
                            MarkDirty(x, y);
                            MarkDirty(x, y + 1);
                            had_mobile = true;
                            continue;
                        }

                        // Density settling: heavier liquid sinks through lighter
                        if (below_st == MaterialState::Liquid &&
                            cell_density > density_lut_[below.material_id] &&
                            !processed_[(y + 1) * w + x]) {
                            int didx = (y + 1) * w + x;
                            terrain.SetCell(x, y + 1, cell);
                            terrain.SetCell(x, y, below);
                            uint8_t tmp = mass_[idx];
                            mass_[idx] = mass_[didx];
                            mass_[didx] = tmp;
                            processed_[didx] = 1;
                            MarkDirty(x, y);
                            MarkDirty(x, y + 1);
                            had_mobile = true;
                            continue;
                        }

                        // Diagonal-down fall
                        int dirs[2] = {-1, 1};
                        if ((x + y) & 1) { dirs[0] = 1; dirs[1] = -1; }

                        bool fell_diag = false;
                        for (int d : dirs) {
                            int nx = x + d;
                            if (nx < 0 || nx >= w) continue;

                            Cell diag = terrain.GetCell(nx, y + 1);
                            Cell side = terrain.GetCell(nx, y);
                            MaterialState diag_st = state_lut_[diag.material_id];
                            MaterialState side_st = state_lut_[side.material_id];

                            bool diag_ok = (diag_st == MaterialState::None || diag_st == MaterialState::Gas ||
                                            (diag_st == MaterialState::Liquid && cell_density > density_lut_[diag.material_id]));
                            bool side_ok = (side_st == MaterialState::None || side_st == MaterialState::Gas ||
                                            side_st == MaterialState::Liquid);

                            if (diag_ok && side_ok) {
                                int didx = (y + 1) * w + nx;
                                if (!processed_[didx]) {
                                    terrain.SetCell(nx, y + 1, cell);
                                    terrain.SetCell(x, y, diag);
                                    uint8_t tmp = mass_[idx];
                                    mass_[idx] = mass_[didx];
                                    mass_[didx] = tmp;
                                    processed_[didx] = 1;
                                    MarkDirty(x, y);
                                    MarkDirty(nx, y + 1);
                                    fell_diag = true;
                                    had_mobile = true;
                                    break;
                                }
                            }
                        }
                        if (fell_diag) continue;
                    }

                    // Horizontal spread (runs on every pass where cell_flow > pass)
                    if (cell_flow <= pass) continue;

                    int dirs[2] = {-1, 1};
                    if ((x + y) & 1) { dirs[0] = 1; dirs[1] = -1; }

                    for (int d : dirs) {
                        int nx = x + d;
                        if (nx < 0 || nx >= w) continue;
                        int nidx = y * w + nx;
                        if (processed_[nidx]) continue;

                        Cell side = terrain.GetCell(nx, y);
                        MaterialState side_st = state_lut_[side.material_id];

                        if (side_st == MaterialState::None || side_st == MaterialState::Gas) {
                            terrain.SetCell(nx, y, cell);
                            terrain.SetCell(x, y, side);
                            uint8_t tmp = mass_[idx];
                            mass_[idx] = mass_[nidx];
                            mass_[nidx] = tmp;
                            processed_[nidx] = 1;
                            MarkDirty(x, y);
                            MarkDirty(nx, y);
                            had_mobile = true;
                            break;
                        }
                    }
                }
            }

            // Deactivation happens in PruneInactiveChunks() after all sims
            // complete — individual sims can't deactivate because they only
            // see their own material kind.
            (void)had_mobile;
        }
    }
}

// ── Gas Simulation ───────────────────────────────────────────────────────────

void TerrainSimulator::SimulateGas(Terrain& terrain, int pass) {
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

            bool had_mobile = false;

            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    int idx = y * w + x;
                    if (processed_[idx]) continue;

                    Cell cell = terrain.GetCell(x, y);
                    if (state_lut_[cell.material_id] != MaterialState::Gas) continue;

                    // Gas is intrinsically mobile — always keep the chunk alive
                    // as long as any gas cell lives here.
                    had_mobile = true;

                    int cell_density = density_lut_[cell.material_id];
                    int cell_rise = rise_rate_lut_[cell.material_id];
                    int cell_disp = dispersion_lut_[cell.material_id];

                    // Dissipation (only on pass 0 to avoid multi-decrement)
                    if (pass == 0) {
                        int lt = lifetime_lut_[cell.material_id];
                        if (lt > 0) {
                            if (mass_[idx] <= 1) {
                                // Dissipate: become air
                                Cell air = {0, cell.background_id};
                                terrain.SetCell(x, y, air);
                                mass_[idx] = 0;
                                processed_[idx] = 1;
                                MarkDirty(x, y);
                                had_mobile = true;
                                continue;
                            }
                            mass_[idx]--;
                        }
                    }

                    // Rise (pass 0 only for vertical, guarded by processed flag)
                    if (pass == 0 && cell_rise > 0 && y > 0) {
                        Cell above = terrain.GetCell(x, y - 1);
                        MaterialState above_st = state_lut_[above.material_id];

                        // Rise into air
                        if (above_st == MaterialState::None) {
                            int didx = (y - 1) * w + x;
                            terrain.SetCell(x, y - 1, cell);
                            terrain.SetCell(x, y, above);
                            uint8_t tmp = mass_[idx];
                            mass_[idx] = mass_[didx];
                            mass_[didx] = tmp;
                            processed_[didx] = 1;
                            MarkDirty(x, y);
                            MarkDirty(x, y - 1);
                            had_mobile = true;
                            continue;
                        }

                        // Rise through lighter gas (density settling upward)
                        if (above_st == MaterialState::Gas &&
                            cell_density < density_lut_[above.material_id] &&
                            !processed_[(y - 1) * w + x]) {
                            int didx = (y - 1) * w + x;
                            terrain.SetCell(x, y - 1, cell);
                            terrain.SetCell(x, y, above);
                            uint8_t tmp = mass_[idx];
                            mass_[idx] = mass_[didx];
                            mass_[didx] = tmp;
                            processed_[didx] = 1;
                            MarkDirty(x, y);
                            MarkDirty(x, y - 1);
                            had_mobile = true;
                            continue;
                        }

                        // Diagonal-up
                        int dirs[2] = {-1, 1};
                        if ((x + y) & 1) { dirs[0] = 1; dirs[1] = -1; }

                        bool rose_diag = false;
                        for (int d : dirs) {
                            int nx = x + d;
                            if (nx < 0 || nx >= w) continue;

                            Cell diag = terrain.GetCell(nx, y - 1);
                            Cell side = terrain.GetCell(nx, y);
                            MaterialState diag_st = state_lut_[diag.material_id];
                            MaterialState side_st = state_lut_[side.material_id];

                            bool diag_ok = (diag_st == MaterialState::None ||
                                            (diag_st == MaterialState::Gas && cell_density < density_lut_[diag.material_id]));
                            bool side_ok = (side_st == MaterialState::None || side_st == MaterialState::Gas);

                            if (diag_ok && side_ok) {
                                int didx = (y - 1) * w + nx;
                                if (!processed_[didx]) {
                                    terrain.SetCell(nx, y - 1, cell);
                                    terrain.SetCell(x, y, diag);
                                    uint8_t tmp = mass_[idx];
                                    mass_[idx] = mass_[didx];
                                    mass_[didx] = tmp;
                                    processed_[didx] = 1;
                                    MarkDirty(x, y);
                                    MarkDirty(nx, y - 1);
                                    rose_diag = true;
                                    had_mobile = true;
                                    break;
                                }
                            }
                        }
                        if (rose_diag) continue;
                    }

                    // Horizontal dispersion (runs on passes where cell_disp > pass)
                    if (cell_disp <= pass) continue;

                    int dirs[2] = {-1, 1};
                    if ((x + y) & 1) { dirs[0] = 1; dirs[1] = -1; }

                    for (int d : dirs) {
                        int nx = x + d;
                        if (nx < 0 || nx >= w) continue;
                        int nidx = y * w + nx;
                        if (processed_[nidx]) continue;

                        Cell side = terrain.GetCell(nx, y);
                        if (state_lut_[side.material_id] == MaterialState::None) {
                            terrain.SetCell(nx, y, cell);
                            terrain.SetCell(x, y, side);
                            uint8_t tmp = mass_[idx];
                            mass_[idx] = mass_[nidx];
                            mass_[nidx] = tmp;
                            processed_[nidx] = 1;
                            MarkDirty(x, y);
                            MarkDirty(nx, y);
                            had_mobile = true;
                            break;
                        }
                    }
                }
            }

            // Deactivation happens in PruneInactiveChunks() after all sims
            // complete — the gas sim only sees gas cells, so it would wrongly
            // deactivate a chunk that still contains falling powder or liquid.
            (void)had_mobile;
        }
    }
}

// ── Chunk Prune ──────────────────────────────────────────────────────────────

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
                    } else if (st == MaterialState::Powder &&
                               gravity_lut_[cell.material_id]) {
                        has_mobile = true;
                    } else if (st == MaterialState::Liquid &&
                               gravity_lut_[cell.material_id]) {
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

// ── Pressure Equalization ────────────────────────────────────────────────────

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

                    // Check horizontal neighbors
                    int dirs[2] = {-1, 1};
                    if ((x + y) & 1) { dirs[0] = 1; dirs[1] = -1; }

                    for (int d : dirs) {
                        int nx = x + d;
                        if (nx < 0 || nx >= w) continue;
                        int nidx = y * w + nx;

                        Cell neighbor = terrain.GetCell(nx, y);
                        MaterialState n_st = state_lut_[neighbor.material_id];

                        if (n_st == MaterialState::None || n_st == MaterialState::Gas) {
                            // Spawn liquid into empty cell if pressure is high enough
                            if (p_self > SPAWN_THRESHOLD && mass_[idx] > MIN_TRANSFER) {
                                uint8_t transfer = static_cast<uint8_t>(std::min(
                                    static_cast<int>(mass_[idx]) / 4,
                                    static_cast<int>(mass_[idx]) - 1));
                                if (transfer < MIN_TRANSFER) continue;

                                // Push neighbor out (gas displaced, air replaced)
                                terrain.SetCell(nx, y, cell);
                                mass_[nidx] = transfer;
                                mass_[idx] -= transfer;

                                // If mass dropped too low, become air
                                if (mass_[idx] == 0) {
                                    terrain.SetCell(x, y, {0, cell.background_id});
                                }

                                MarkDirty(x, y);
                                MarkDirty(nx, y);
                            }
                        } else if (n_st == MaterialState::Liquid &&
                                   neighbor.material_id == cell.material_id) {
                            // Equalize mass between same-type liquid neighbors
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
                                    mass_[idx] -= transfer;
                                    mass_[nidx] += transfer;

                                    if (mass_[idx] == 0) {
                                        terrain.SetCell(x, y, {0, cell.background_id});
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
