#include "TerrainGen.h"
#include <cmath>
#include <vector>

// ── Noise utilities ────────────────────────────────────────────────────────

static float hash2(int x, int y) {
    int n = x + y * 57;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

static float smooth_noise(float x, float y) {
    int ix = (int)x; int iy = (int)y;
    float fx = x - ix; float fy = y - iy;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    return hash2(ix,   iy)*(1-fx)*(1-fy) + hash2(ix+1, iy)*fx*(1-fy)
         + hash2(ix,   iy+1)*(1-fx)*fy   + hash2(ix+1, iy+1)*fx*fy;
}

// ── Generation ─────────────────────────────────────────────────────────────

void terrain_generate(TerrainFacade& terrain) {
    int w = terrain.cells_w();
    int h = terrain.cells_h();

    // ── Pass 1: Height maps ──────────────────────────────────────────────

    std::vector<int> surface_map(w), rock_map(w);

    for (int cx = 0; cx < w; cx++) {
        // Dramatic multi-octave surface: large hills + medium bumps + fine detail
        float surface =
            h * 0.28f
            + smooth_noise(cx * 0.018f, 0.0f)  * 32.0f   // large rolling hills
            + smooth_noise(cx * 0.055f, 5.0f)   * 12.0f   // medium bumps
            + smooth_noise(cx * 0.15f,  15.0f)  *  4.0f;  // fine ripple
        surface_map[cx] = (int)surface;

        float rock =
            h * 0.68f
            + smooth_noise(cx * 0.025f, 80.0f) * 18.0f
            + smooth_noise(cx * 0.07f,  90.0f) *  6.0f;
        rock_map[cx] = (int)rock;
    }

    // ── Pass 2: Base fill ────────────────────────────────────────────────

    for (int cy = 0; cy < h; cy++) {
        for (int cx = 0; cx < w; cx++) {
            MaterialID mat;
            if      (cy < surface_map[cx]) mat = MaterialID::AIR;
            else if (cy >= rock_map[cx])   mat = MaterialID::ROCK;
            else                           mat = MaterialID::DIRT;
            terrain.set_material(cx, cy, mat);
        }
    }

    // ── Pass 3: Caves ────────────────────────────────────────────────────
    // "Worm tunnels" via |noise| < threshold — thin thread-like cavities

    for (int cy = 0; cy < h; cy++) {
        for (int cx = 0; cx < w; cx++) {
            if (terrain.get_material(cx, cy) != MaterialID::DIRT) continue;

            float depth_ratio = (cy - surface_map[cx]) /
                                 (float)(rock_map[cx] - surface_map[cx] + 1);
            if (depth_ratio < 0.28f) continue; // no caves near the surface

            // Multiply two noise fields → caves only where both are "up"
            float n1 = smooth_noise(cx * 0.045f, cy * 0.06f);
            float n2 = smooth_noise(cx * 0.06f + 37.0f, cy * 0.04f + 19.0f);
            if (n1 * n2 > 0.38f) {
                terrain.set_material(cx, cy, MaterialID::EMPTY);
            }
        }
    }

    // ── Pass 4: Ore (per-cell noise threshold) ───────────────────────────
    // Scattered vein style: fbm noise threshold per cell.

    auto fbm = [&](float x, float y) {
        return 0.6f * smooth_noise(x * 0.08f, y * 0.08f)
             + 0.4f * smooth_noise(x * 0.2f,  y * 0.2f);
    };

    for (int cy = 0; cy < h; cy++) {
        for (int cx = 0; cx < w; cx++) {
            MaterialID mat = terrain.get_material(cx, cy);
            if (mat != MaterialID::DIRT && mat != MaterialID::ROCK) continue;

            float sky_end = (float)surface_map[cx];
            float depth_ratio = (cy - sky_end) / (h - sky_end);
            if (depth_ratio < 0.4f) continue; // only deeper cells

            float ore_noise = fbm((float)cx, (float)cy);
            if (ore_noise > 0.62f)
                terrain.set_material(cx, cy, MaterialID::GOLD_ORE);
        }
    }

    // ── Pass 5: Water pockets ────────────────────────────────────────────

    auto place_blob = [&](int bx, int by, int radius, MaterialID ore) {
        int r2 = radius * radius;
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx*dx + dy*dy > r2) continue;
                int nx = bx + dx, ny = by + dy;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                MaterialID m = terrain.get_material(nx, ny);
                if (m == MaterialID::DIRT || m == MaterialID::ROCK)
                    terrain.set_material(nx, ny, ore);
            }
        }
    };

    for (int i = 0; i < 4; i++) {
        float fx = (hash2(i * 31 + 11, 7) * 0.5f + 0.5f);
        int bx = (int)(fx * (w - 1));
        bx = bx < 0 ? 0 : (bx >= w ? w-1 : bx);

        int surf = surface_map[bx], rock = rock_map[bx];
        int by = surf + (int)((rock - surf) * 0.55f) + i * 8;
        by = by < 0 ? 0 : (by >= h ? h-1 : by);

        int radius = 4 + (int)((hash2(i + 50, 3) * 0.5f + 0.5f) * 5.0f);
        place_blob(bx, by, radius, MaterialID::WATER);
    }

    // ── Mark all chunks dirty ────────────────────────────────────────────

    for (int cy = 0; cy < terrain.chunks_y(); cy++)
        for (int cx = 0; cx < terrain.chunks_x(); cx++) {
            auto& c = terrain.get_chunk(cx, cy);
            c.dirty_visual    = true;
            c.dirty_collision = true;
        }
}
