#include "terrain/MapGenerator.h"
#include <cmath>
#include <random>

Terrain MapGenerator::GenerateFlat(int width, int height, uint32_t seed, const DefinitionRegistry& registry) {
    Terrain terrain(width, height);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);

    // Look up material runtime IDs from the registry
    auto lookup = [&](const std::string& id) -> MaterialID {
        auto* mat = registry.GetMaterial(id);
        return mat ? mat->runtime_id : 0;
    };

    MaterialID air_id     = lookup("base:Air");
    MaterialID dirt_id    = lookup("base:Dirt");
    MaterialID rock_id    = lookup("base:Rock");
    MaterialID sand_id    = lookup("base:Sand");
    MaterialID gold_id    = lookup("base:GoldOre");
    MaterialID coal_id    = lookup("base:CoalOre");
    MaterialID water_id   = lookup("base:Water");

    int base_surface = height * 35 / 100;
    int rock_depth = 40;

    // Generate surface heightmap with noise
    std::vector<int> surface(width);
    for (int x = 0; x < width; x++) {
        float wave = std::sin(x * 0.02f) * 8.0f
                   + std::sin(x * 0.005f) * 20.0f
                   + std::sin(x * 0.05f) * 3.0f;
        float jitter = noise_dist(rng) * 2.0f;
        surface[x] = base_surface + static_cast<int>(wave + jitter);
    }

    // Fill cells
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int surf = surface[x];
            int depth_from_surface = y - surf;

            if (y < surf) {
                terrain.SetMaterial(x, y, air_id);
            } else if (depth_from_surface < 4) {
                if (noise_dist(rng) > 0.7f) {
                    terrain.SetMaterial(x, y, sand_id);
                } else {
                    terrain.SetMaterial(x, y, dirt_id);
                }
            } else if (depth_from_surface < rock_depth) {
                terrain.SetMaterial(x, y, dirt_id);
            } else {
                terrain.SetMaterial(x, y, rock_id);
            }
        }
    }

    // Sprinkle gold ore veins in rock layer
    std::uniform_int_distribution<int> x_dist(0, width - 1);
    std::uniform_int_distribution<int> y_dist(0, height - 1);
    int vein_count = (width * height) / 8000;

    for (int v = 0; v < vein_count; v++) {
        int cx = x_dist(rng);
        int cy = y_dist(rng);
        if (cy < base_surface + rock_depth) continue;

        MaterialID vein_mat = (v % 3 == 0) ? coal_id : gold_id;
        int radius = 3 + (rng() % 6);
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx * dx + dy * dy <= radius * radius) {
                    int px = cx + dx;
                    int py = cy + dy;
                    if (terrain.InBounds(px, py) &&
                        terrain.GetCell(px, py).material_id == rock_id) {
                        terrain.SetMaterial(px, py, vein_mat);
                    }
                }
            }
        }
    }

    // Add a small lake on the surface
    int lake_x = width / 3;
    int lake_width = 60;
    int lake_depth = 20;
    for (int x = lake_x; x < lake_x + lake_width && x < width; x++) {
        int surf = surface[std::min(x, width - 1)];
        for (int y = surf; y < surf + lake_depth && y < height; y++) {
            terrain.SetMaterial(x, y, water_id);
        }
    }

    return terrain;
}
