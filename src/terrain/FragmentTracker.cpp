#include "terrain/FragmentTracker.h"
#include "terrain/DynamicBodyManager.h"
#include "package/DefinitionRegistry.h"
#include "package/MaterialDef.h"
#include <algorithm>
#include <queue>
#include <cmath>

FragmentTracker::FragmentTracker(const DefinitionRegistry& registry)
    : registry_(registry)
{
    solid_lut_.resize(256, false);
    for (int i = 0; i < 256; i++) {
        auto* mat = registry.GetMaterialByRuntimeID(static_cast<MaterialID>(i));
        if (mat && mat->state == MaterialState::Solid)
            solid_lut_[i] = true;
    }
}

// ── Crack propagation ─────────────────────────────────────────────────────────

void FragmentTracker::ApplyDamage(
    Terrain& terrain, uint8_t* crack,
    DynamicBodyManager& dbm,
    int sx, int sy, int damage)
{
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();
    if (sx < 0 || sx >= w || sy < 0 || sy >= h) return;

    Cell cell = terrain.GetCell(sx, sy);
    if (!solid_lut_[cell.material_id]) return;

    int idx = sy * w + sx;
    int new_crack = static_cast<int>(crack[idx]) + damage;
    crack[idx] = static_cast<uint8_t>(std::min(new_crack, 255));

    // Propagate cracks radially using 8 Bresenham rays
    int n_rays = 8;
    float len  = static_cast<float>(damage) * 0.6f;

    for (int r = 0; r < n_rays; r++) {
        float angle = static_cast<float>(r) / static_cast<float>(n_rays) * 6.283185f;
        int tx = sx + static_cast<int>(std::cos(angle) * len);
        int ty = sy + static_cast<int>(std::sin(angle) * len);

        int dx2 = tx - sx, dy2 = ty - sy;
        int sign_x = (dx2 > 0) ? 1 : (dx2 < 0) ? -1 : 0;
        int sign_y = (dy2 > 0) ? 1 : (dy2 < 0) ? -1 : 0;
        int adx = std::abs(dx2), ady = std::abs(dy2);
        int err = adx - ady;
        int rx = sx, ry = sy;

        for (int step = 0; step < static_cast<int>(len); step++) {
            int e2 = 2 * err;
            int nx = rx, ny = ry;
            if (e2 > -ady) { err -= ady; nx += sign_x; }
            if (e2 <  adx) { err += adx; ny += sign_y; }
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) break;
            Cell nc = terrain.GetCell(nx, ny);
            if (!solid_lut_[nc.material_id]) break;
            int nidx = ny * w + nx;
            int nc2  = static_cast<int>(crack[nidx]) + damage / 2;
            crack[nidx] = static_cast<uint8_t>(std::min(nc2, 255));
            rx = nx; ry = ny;
        }
    }

    // After crack update, check if any region near the damage site is fully
    // isolated by cracks.  Use DynamicBodyManager to handle the actual isolation.
    int scan_r = static_cast<int>(len) + 2;
    dbm.ScanForFloatingGroups(terrain,
                               sx - scan_r, sy - scan_r,
                               2 * scan_r + 1, 2 * scan_r + 1);
}

// ── Isolation detection ───────────────────────────────────────────────────────

bool FragmentTracker::FindIsolatedFragment(
    const Terrain& terrain, const uint8_t* crack,
    int sx, int sy,
    std::vector<std::pair<int,int>>& fragment_cells) const
{
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();
    if (sx < 0 || sx >= w || sy < 0 || sy >= h) return false;

    Cell cell = terrain.GetCell(sx, sy);
    if (!solid_lut_[cell.material_id]) return false;
    if (crack[sy * w + sx] < CRACK_THRESHOLD) return false;

    // BFS: collect connected cells where all boundary neighbours are either
    // high-crack or non-solid.
    std::vector<bool> visited(w * h, false);
    std::queue<int> q;
    int start = sy * w + sx;
    q.push(start);
    visited[start] = true;

    static const int DX[] = {1, -1, 0,  0};
    static const int DY[] = {0,  0, 1, -1};

    bool touches_edge = false;

    while (!q.empty()) {
        int cur = q.front(); q.pop();
        int cx = cur % w, cy = cur / w;
        fragment_cells.emplace_back(cx, cy);

        if (cx == 0 || cx == w-1 || cy == 0 || cy == h-1) {
            touches_edge = true; // anchored at map boundary — not floating
        }

        for (int d = 0; d < 4; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            int nidx = ny * w + nx;
            if (visited[nidx]) continue;
            Cell nc = terrain.GetCell(nx, ny);
            if (!solid_lut_[nc.material_id]) continue;
            if (crack[nidx] < CRACK_THRESHOLD) {
                // Connected to a non-cracked solid — not isolated
                fragment_cells.clear();
                return false;
            }
            visited[nidx] = true;
            q.push(nidx);
        }
    }

    if (touches_edge) { fragment_cells.clear(); return false; }
    return !fragment_cells.empty();
}
