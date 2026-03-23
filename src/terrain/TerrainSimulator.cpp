#include "terrain/TerrainSimulator.h"
#include "package/MaterialDef.h"
#include <algorithm>

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

    for (int i = 0; i < 256; i++) {
        auto* mat = registry.GetMaterialByRuntimeID(static_cast<MaterialID>(i));
        if (mat) {
            state_lut_[i] = mat->state;
            gravity_lut_[i] = mat->gravity;
            flow_lut_[i] = mat->flow_rate;
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

    // Mark the destination chunk and its neighbor below as active for next tick
    int cx = x / SIM_CHUNK;
    int cy = y / SIM_CHUNK;
    if (cx >= 0 && cx < chunks_x_ && cy >= 0 && cy < chunks_y_) {
        chunk_active_[ChunkIndex(cx, cy)] = true;
        // Activate chunk below too (powder/liquid falls into it)
        if (cy + 1 < chunks_y_) chunk_active_[ChunkIndex(cx, cy + 1)] = true;
        // Activate neighbor chunks for horizontal liquid spread
        if (cx > 0) chunk_active_[ChunkIndex(cx - 1, cy)] = true;
        if (cx + 1 < chunks_x_) chunk_active_[ChunkIndex(cx + 1, cy)] = true;
    }
}

void TerrainSimulator::NotifyModified(int rx, int ry, int rw, int rh) {
    // External modification (dig/blast) — activate affected chunks + neighbors
    int cx0 = std::max(0, (rx - 1) / SIM_CHUNK);
    int cy0 = std::max(0, (ry - 1) / SIM_CHUNK);
    int cx1 = std::min(chunks_x_ - 1, (rx + rw) / SIM_CHUNK);
    int cy1 = std::min(chunks_y_ - 1, (ry + rh + SIM_CHUNK) / SIM_CHUNK);  // extra below

    for (int cy = cy0; cy <= cy1; cy++) {
        for (int cx = cx0; cx <= cx1; cx++) {
            chunk_active_[ChunkIndex(cx, cy)] = true;
        }
    }
}

void TerrainSimulator::ScanActiveChunks(const Terrain& terrain) {
    // Full scan to find which chunks contain movable (powder/liquid with gravity) cells
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();
    chunks_x_ = (w + SIM_CHUNK - 1) / SIM_CHUNK;
    chunks_y_ = (h + SIM_CHUNK - 1) / SIM_CHUNK;
    chunk_active_.assign(chunks_x_ * chunks_y_, false);

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
                    if ((st == MaterialState::Powder || st == MaterialState::Liquid) &&
                        gravity_lut_[cell.material_id]) {
                        active = true;
                    }
                }
            }
            chunk_active_[ChunkIndex(cx, cy)] = active;
        }
    }
}

void TerrainSimulator::Update(Terrain& terrain) {
    any_dirty_ = false;
    dirty_rects_.clear();

    tick_counter_++;
    if (tick_counter_ < SIM_INTERVAL) return;
    tick_counter_ = 0;

    // First tick: do a full scan to find active chunks
    if (needs_full_scan_) {
        ScanActiveChunks(terrain);
        needs_full_scan_ = false;
    }

    SimulatePowder(terrain);
    SimulateLiquid(terrain);

    if (any_dirty_) {
        dirty_rects_.push_back({
            std::max(0, dirty_min_x_ - 1),
            std::max(0, dirty_min_y_ - 1),
            std::min(terrain.GetWidth(), dirty_max_x_ + 2) - std::max(0, dirty_min_x_ - 1),
            std::min(terrain.GetHeight(), dirty_max_y_ + 2) - std::max(0, dirty_min_y_ - 1)
        });
    }
}

void TerrainSimulator::SimulatePowder(Terrain& terrain) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    // Only scan active chunks, bottom-to-top
    for (int cy = chunks_y_ - 1; cy >= 0; cy--) {
        for (int cx = 0; cx < chunks_x_; cx++) {
            if (!chunk_active_[ChunkIndex(cx, cy)]) continue;

            int x0 = cx * SIM_CHUNK;
            int y0 = cy * SIM_CHUNK;
            int x1 = std::min(x0 + SIM_CHUNK, w);
            int y1 = std::min(y0 + SIM_CHUNK, h - 1);  // -1: need room below

            // Track if this chunk did any work
            bool had_activity = false;

            for (int y = y1 - 1; y >= y0; y--) {
                for (int x = x0; x < x1; x++) {
                    Cell cell = terrain.GetCell(x, y);
                    if (state_lut_[cell.material_id] != MaterialState::Powder ||
                        !gravity_lut_[cell.material_id]) continue;

                    Cell below = terrain.GetCell(x, y + 1);
                    MaterialState below_st = state_lut_[below.material_id];

                    if (below_st == MaterialState::None || below_st == MaterialState::Liquid) {
                        terrain.SetCell(x, y + 1, cell);
                        terrain.SetCell(x, y, below);
                        MarkDirty(x, y);
                        MarkDirty(x, y + 1);
                        had_activity = true;
                        continue;
                    }

                    int dirs[2] = {-1, 1};
                    if ((x + y) & 1) { dirs[0] = 1; dirs[1] = -1; }

                    for (int d : dirs) {
                        int nx = x + d;
                        if (nx < 0 || nx >= w) continue;

                        Cell diag = terrain.GetCell(nx, y + 1);
                        MaterialState diag_st = state_lut_[diag.material_id];
                        Cell side = terrain.GetCell(nx, y);
                        MaterialState side_st = state_lut_[side.material_id];

                        if ((diag_st == MaterialState::None || diag_st == MaterialState::Liquid) &&
                            (side_st == MaterialState::None || side_st == MaterialState::Liquid)) {
                            terrain.SetCell(nx, y + 1, cell);
                            terrain.SetCell(x, y, diag);
                            MarkDirty(x, y);
                            MarkDirty(nx, y + 1);
                            had_activity = true;
                            break;
                        }
                    }
                }
            }

            // If no activity, deactivate this chunk (unless re-activated by MarkDirty)
            if (!had_activity) {
                chunk_active_[ChunkIndex(cx, cy)] = false;
            }
        }
    }
}

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

            bool had_activity = false;

            for (int y = y1 - 1; y >= y0; y--) {
                for (int x = x0; x < x1; x++) {
                    Cell cell = terrain.GetCell(x, y);
                    if (state_lut_[cell.material_id] != MaterialState::Liquid ||
                        !gravity_lut_[cell.material_id]) continue;

                    Cell below = terrain.GetCell(x, y + 1);
                    if (state_lut_[below.material_id] == MaterialState::None) {
                        terrain.SetCell(x, y + 1, cell);
                        terrain.SetCell(x, y, below);
                        MarkDirty(x, y);
                        MarkDirty(x, y + 1);
                        had_activity = true;
                        continue;
                    }

                    int flow = std::max(1, flow_lut_[cell.material_id]);
                    int dirs[2] = {-1, 1};
                    if ((x + y) & 1) { dirs[0] = 1; dirs[1] = -1; }

                    for (int d : dirs) {
                        bool spread = false;
                        for (int step = 1; step <= flow; step++) {
                            int nx = x + d * step;
                            if (nx < 0 || nx >= w) break;

                            Cell side = terrain.GetCell(nx, y);
                            if (state_lut_[side.material_id] != MaterialState::None) break;

                            terrain.SetCell(nx, y, cell);
                            terrain.SetCell(x, y, side);
                            MarkDirty(x, y);
                            MarkDirty(nx, y);
                            spread = true;
                            had_activity = true;
                            break;
                        }
                        if (spread) break;
                    }
                }
            }

            if (!had_activity) {
                chunk_active_[ChunkIndex(cx, cy)] = false;
            }
        }
    }
}
