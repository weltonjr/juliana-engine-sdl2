#include "TerrainGen.h"
#include <cmath>
#include <cstdlib>

// Simple deterministic hash for 2D coordinates
static float hash2(int x, int y) {
    int n = x + y * 57;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

// Smooth noise (bilinear interpolation of hash)
static float smooth_noise(float x, float y) {
    int ix = (int)x;
    int iy = (int)y;
    float fx = x - ix;
    float fy = y - iy;
    // Smoothstep
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);

    float v00 = hash2(ix,   iy);
    float v10 = hash2(ix+1, iy);
    float v01 = hash2(ix,   iy+1);
    float v11 = hash2(ix+1, iy+1);

    return v00*(1-fx)*(1-fy) + v10*fx*(1-fy) +
           v01*(1-fx)*fy    + v11*fx*fy;
}

// Fractal noise (2 octaves is enough)
static float fbm(float x, float y) {
    return 0.6f * smooth_noise(x * 0.08f, y * 0.08f)
         + 0.4f * smooth_noise(x * 0.2f,  y * 0.2f);
}

void terrain_generate(TerrainFacade& terrain) {
    int w = terrain.cells_w();
    int h = terrain.cells_h();

    // Sky/dirt boundary row: ~20% from top, with noise variation
    float base_sky_end    = h * 0.20f;
    // Dirt/rock boundary row: ~70% from top
    float base_rock_start = h * 0.70f;

    for (int cy = 0; cy < h; ++cy) {
        for (int cx = 0; cx < w; ++cx) {
            // Surface variation using noise
            float surface_offset = smooth_noise(cx * 0.05f, 0.0f) * 8.0f;
            float sky_end    = base_sky_end   + surface_offset;
            float rock_start = base_rock_start + smooth_noise(cx * 0.03f, 100.0f) * 6.0f;

            MaterialID mat;
            if (cy < (int)sky_end) {
                mat = MaterialID::AIR;
            } else if (cy >= (int)rock_start) {
                mat = MaterialID::ROCK;
            } else {
                mat = MaterialID::DIRT;
            }

            // Gold ore veins in the dirt/rock transition zone
            if (mat == MaterialID::DIRT || mat == MaterialID::ROCK) {
                float depth_ratio = (cy - sky_end) / (h - sky_end);
                if (depth_ratio > 0.4f) { // only in lower 60% of solid terrain
                    float ore_noise = fbm((float)cx, (float)cy);
                    // Threshold: ~8% of cells become ore in this zone
                    if (ore_noise > 0.62f) {
                        mat = MaterialID::GOLD_ORE;
                    }
                }
            }

            terrain.set_material(cx, cy, mat);
        }
    }

    // Mark all chunks clean after generation (set_material dirtied them all)
    for (int cy = 0; cy < terrain.chunks_y(); ++cy)
        for (int cx = 0; cx < terrain.chunks_x(); ++cx) {
            auto& chunk = terrain.get_chunk(cx, cy);
            chunk.dirty_visual    = true;  // renderer will rebuild on first draw
            chunk.dirty_collision = true;
        }
}
