#include "terrain/MapGenerator.h"
#include "package/MaterialDef.h"
#include "package/BackgroundDef.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

// ---- Shape generators ----

std::vector<int> MapGenerator::GenerateFlatShape(int width, int height, std::mt19937& rng, const MapShapeParams& params) {
    float surface_level = params.Get("surface_level", 0.35f);
    float roughness = params.Get("roughness", 0.5f);

    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
    int base_surface = static_cast<int>(height * surface_level);

    std::vector<int> surface(width);
    for (int x = 0; x < width; x++) {
        float wave = std::sin(x * 0.02f) * 8.0f
                   + std::sin(x * 0.005f) * 20.0f
                   + std::sin(x * 0.05f) * 3.0f;
        float jitter = noise(rng) * 2.0f * roughness;
        surface[x] = base_surface + static_cast<int>(wave * roughness + jitter);
        surface[x] = std::clamp(surface[x], 2, height - 2);
    }
    return surface;
}

std::vector<int> MapGenerator::GenerateIslandShape(int width, int height, std::mt19937& rng, const MapShapeParams& params) {
    float sea_level = params.Get("sea_level", 0.6f);
    float terrain_height = params.Get("terrain_height", 0.4f);
    float roughness = params.Get("roughness", 0.5f);

    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
    int sea_y = static_cast<int>(height * sea_level);

    std::vector<int> surface(width);
    float cx = width / 2.0f;

    for (int x = 0; x < width; x++) {
        // Island profile: raised in center, sinks at edges
        float dist = std::abs(x - cx) / cx;  // 0 at center, 1 at edges
        float island = 1.0f - dist * dist;     // Parabolic island
        island = std::max(0.0f, island);

        float wave = std::sin(x * 0.02f) * 5.0f + std::sin(x * 0.05f) * 2.0f;
        float h = island * terrain_height * height + wave * roughness + noise(rng) * roughness;

        surface[x] = sea_y - static_cast<int>(h);
        surface[x] = std::clamp(surface[x], 2, height - 2);
    }
    return surface;
}

std::vector<int> MapGenerator::GenerateMountainShape(int width, int height, std::mt19937& rng, const MapShapeParams& params) {
    float peak_height = params.Get("peak_height", 0.7f);
    float slope_steepness = params.Get("slope_steepness", 0.5f);
    float roughness = params.Get("roughness", 0.5f);

    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
    float cx = width / 2.0f;

    std::vector<int> surface(width);
    for (int x = 0; x < width; x++) {
        float dist = std::abs(x - cx) / cx;
        float mountain = std::pow(1.0f - dist, 1.0f + slope_steepness * 2.0f);
        float wave = std::sin(x * 0.03f) * 4.0f + std::sin(x * 0.07f) * 2.0f;

        int base = static_cast<int>(height * 0.8f);
        surface[x] = base - static_cast<int>(mountain * peak_height * height * 0.6f + wave * roughness + noise(rng));
        surface[x] = std::clamp(surface[x], 2, height - 2);
    }
    return surface;
}

std::vector<int> MapGenerator::GenerateBowlShape(int width, int height, std::mt19937& rng, const MapShapeParams& params) {
    float rim_height = params.Get("rim_height", 0.4f);
    float floor_depth = params.Get("floor_depth", 0.6f);
    float roughness = params.Get("roughness", 0.5f);

    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
    float cx = width / 2.0f;

    std::vector<int> surface(width);
    for (int x = 0; x < width; x++) {
        float dist = std::abs(x - cx) / cx;
        // Bowl: low center, high rim
        float bowl = dist * dist;  // 0 at center, 1 at edges
        float h = rim_height * bowl + floor_depth * (1.0f - bowl);
        float wave = std::sin(x * 0.02f) * 3.0f;

        surface[x] = static_cast<int>(height * (1.0f - h) + wave * roughness + noise(rng));
        surface[x] = std::clamp(surface[x], 2, height - 2);
    }
    return surface;
}

// ---- Material assignment ----

void MapGenerator::AssignMaterials(Terrain& terrain, const std::vector<int>& surface,
                                    const std::vector<MaterialRule>& rules,
                                    const DefinitionRegistry& registry) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();

    auto lookup_mat = [&](const std::string& id) -> MaterialID {
        auto* m = registry.GetMaterial(id);
        return m ? m->runtime_id : 0;
    };
    auto lookup_bg = [&](const std::string& id) -> BackgroundID {
        auto* b = registry.GetBackground(id);
        return b ? b->runtime_id : 0;
    };

    // Default IDs
    MaterialID air_id = lookup_mat("base:Air");
    BackgroundID sky_bg = lookup_bg("base:Sky");

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int surf = surface[std::min(x, w - 1)];
            int depth_from_surface = y - surf;
            bool is_solid = (y >= surf);

            MaterialID mat = air_id;
            BackgroundID bg = sky_bg;
            bool assigned = false;

            for (auto& rule : rules) {
                bool match = false;

                if (rule.rule == "above_surface" && !is_solid) {
                    match = true;
                } else if (rule.rule == "below_sea_level_and_empty" && !is_solid) {
                    // Not implemented yet (need sea_level param)
                } else if (rule.rule == "surface_layer" && is_solid && depth_from_surface < rule.depth) {
                    match = true;
                } else if (rule.rule == "deep" && is_solid && depth_from_surface >= rule.min_depth) {
                    match = true;
                } else if (rule.rule == "fill" && is_solid) {
                    match = true;
                }

                if (match) {
                    mat = lookup_mat(rule.material_id);
                    if (!rule.background_id.empty()) {
                        bg = lookup_bg(rule.background_id);
                    }
                    assigned = true;
                    break;
                }
            }

            terrain.SetCell(x, y, {mat, bg});
        }
    }
}

// ---- Features ----

void MapGenerator::GenerateCaves(Terrain& terrain, const std::vector<int>& surface,
                                  const FeatureConfig& config,
                                  const DefinitionRegistry& registry, std::mt19937& rng) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();
    auto* air = registry.GetMaterial("base:Air");
    if (!air) return;

    int count = config.count > 0 ? config.count :
                static_cast<int>(w * h * config.density / 1000.0f);

    std::uniform_int_distribution<int> x_dist(20, w - 20);

    for (int c = 0; c < count; c++) {
        int cx = x_dist(rng);
        int surf = surface[std::min(cx, w - 1)];
        int min_y = surf + 15;
        if (min_y >= h - 10) continue;
        std::uniform_int_distribution<int> y_dist(min_y, h - 10);
        int cy = y_dist(rng);

        int rx = config.min_size + static_cast<int>(rng() % (config.max_size - config.min_size + 1));
        int ry = rx * 2 / 3;

        for (int dy = -ry; dy <= ry; dy++) {
            for (int dx = -rx; dx <= rx; dx++) {
                float nx = static_cast<float>(dx) / static_cast<float>(rx);
                float ny = static_cast<float>(dy) / static_cast<float>(ry);
                if (nx * nx + ny * ny > 1.0f) continue;
                int px = cx + dx;
                int py = cy + dy;
                if (terrain.InBounds(px, py)) {
                    terrain.SetMaterial(px, py, air->runtime_id);
                }
            }
        }
    }
}

void MapGenerator::GenerateOreVeins(Terrain& terrain, const std::vector<int>& surface,
                                     const FeatureConfig& config,
                                     const DefinitionRegistry& registry, std::mt19937& rng) {
    int w = terrain.GetWidth();
    int h = terrain.GetHeight();
    auto* ore = registry.GetMaterial(config.material);
    if (!ore) return;

    int count = config.count > 0 ? config.count :
                static_cast<int>(w * h * config.density / 100.0f);

    std::uniform_int_distribution<int> x_dist(0, w - 1);
    std::uniform_int_distribution<int> y_dist(0, h - 1);

    for (int v = 0; v < count; v++) {
        int cx = x_dist(rng);
        int cy = y_dist(rng);
        int surf = surface[std::min(cx, w - 1)];

        // Zone check
        if (config.zone == "rock" && cy < surf + 40) continue;
        if (config.zone == "surface" && (cy < surf || cy > surf + 40)) continue;
        if (config.zone == "underground" && cy < surf + 10) continue;

        // Only place in solid terrain
        auto* existing = registry.GetMaterialByRuntimeID(terrain.GetCell(cx, cy).material_id);
        if (!existing || existing->state != MaterialState::Solid) continue;

        int radius = std::max(1, config.vein_radius / 2) + static_cast<int>(rng() % config.vein_radius);
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx * dx + dy * dy > radius * radius) continue;
                int px = cx + dx;
                int py = cy + dy;
                if (terrain.InBounds(px, py)) {
                    auto* target = registry.GetMaterialByRuntimeID(terrain.GetCell(px, py).material_id);
                    if (target && target->state == MaterialState::Solid) {
                        terrain.SetMaterial(px, py, ore->runtime_id);
                    }
                }
            }
        }
    }
}

void MapGenerator::GenerateLakes(Terrain& terrain, const std::vector<int>& surface,
                                  const FeatureConfig& config,
                                  const DefinitionRegistry& registry, std::mt19937& rng) {
    int w = terrain.GetWidth();
    auto* water = registry.GetMaterial("base:Water");
    auto* air = registry.GetMaterial("base:Air");
    if (!water || !air) return;

    int count = std::max(1, config.count);
    std::uniform_int_distribution<int> x_dist(50, w - 50);

    for (int l = 0; l < count; l++) {
        int lx = x_dist(rng);
        int lake_w = config.min_size + static_cast<int>(rng() % (config.max_size - config.min_size + 1));
        int lake_d = lake_w / 3;

        for (int x = lx; x < lx + lake_w && x < w; x++) {
            int surf = surface[std::min(x, w - 1)];
            for (int y = surf; y < surf + lake_d && y < terrain.GetHeight(); y++) {
                terrain.SetMaterial(x, y, water->runtime_id);
            }
        }
    }
}

void MapGenerator::ApplyFeatures(Terrain& terrain, const std::vector<int>& surface,
                                  const std::vector<FeatureConfig>& features,
                                  const DefinitionRegistry& registry, std::mt19937& rng) {
    for (auto& feat : features) {
        if (feat.type == "caves") {
            GenerateCaves(terrain, surface, feat, registry, rng);
        } else if (feat.type == "ore_veins") {
            GenerateOreVeins(terrain, surface, feat, registry, rng);
        } else if (feat.type == "lakes") {
            GenerateLakes(terrain, surface, feat, registry, rng);
        } else {
            std::printf("  Unknown feature type: %s\n", feat.type.c_str());
        }
    }
}

// ---- Spawn positions ----

int MapGenerator::FindSurfaceY(const Terrain& terrain, int x, const DefinitionRegistry& registry) {
    for (int y = 0; y < terrain.GetHeight(); y++) {
        auto* mat = registry.GetMaterialByRuntimeID(terrain.GetCell(x, y).material_id);
        if (mat && (mat->state == MaterialState::Solid || mat->state == MaterialState::Powder)) {
            return y;
        }
    }
    return terrain.GetHeight() - 1;
}

std::vector<SpawnPosition> MapGenerator::FindSpawnPositions(
    const Terrain& terrain, const DefinitionRegistry& registry,
    const std::vector<PlayerSlot>& slots)
{
    std::vector<SpawnPosition> positions;
    int w = terrain.GetWidth();

    for (auto& slot : slots) {
        if (slot.type == "none") {
            positions.push_back({0, 0});
            continue;
        }

        SpawnPosition best = {w / 2, 0};
        best.y = FindSurfaceY(terrain, best.x, registry) - 21;

        // Try to find a good spawn position
        int best_score = -1;
        int search_step = std::max(1, w / 50);

        for (int x = 30; x < w - 30; x += search_step) {
            int sy = FindSurfaceY(terrain, x, registry);

            // Check flatness
            int flat_count = 0;
            for (int fx = x - slot.spawn.constraints.min_flat_width / 2;
                 fx < x + slot.spawn.constraints.min_flat_width / 2; fx++) {
                if (fx < 0 || fx >= w) break;
                int fy = FindSurfaceY(terrain, fx, registry);
                if (std::abs(fy - sy) <= 2) flat_count++;
            }

            // Check sky above
            int sky_above = 0;
            for (int cy = sy - 1; cy >= 0 && sky_above < slot.spawn.constraints.min_sky_above + 10; cy--) {
                auto* mat = registry.GetMaterialByRuntimeID(terrain.GetCell(x, cy).material_id);
                if (mat && mat->state == MaterialState::None) sky_above++;
                else break;
            }

            // Check water avoidance
            if (slot.spawn.constraints.avoid_water) {
                auto* mat = registry.GetMaterialByRuntimeID(terrain.GetCell(x, sy).material_id);
                if (mat && mat->state == MaterialState::Liquid) continue;
            }

            // Check distance from other spawns
            bool too_close = false;
            for (auto& prev : positions) {
                int dx = prev.x - x;
                if (std::abs(dx) < slot.spawn.constraints.min_player_distance) {
                    too_close = true;
                    break;
                }
            }
            if (too_close) continue;

            int score = flat_count * 10 + sky_above;
            if (score > best_score) {
                best_score = score;
                best.x = x;
                best.y = sy - 21;
            }
        }

        positions.push_back(best);
    }

    return positions;
}

Terrain MapGenerator::GenerateFromScenario(const ScenarioDef& scenario, const DefinitionRegistry& registry,
                                            uint32_t* seed_used_out) {
    int w = scenario.map.width;
    int h = scenario.map.height;
    uint32_t seed = scenario.map.seed;
    if (seed == 0) {
        seed = static_cast<uint32_t>(std::random_device{}());
    }
    if (seed_used_out) *seed_used_out = seed;
    std::mt19937 rng(seed);

    Terrain terrain(w, h);

    // Pass 1: Generate shape (surface heightmap)
    std::vector<int> surface;
    if (scenario.map.shape == "island") {
        surface = GenerateIslandShape(w, h, rng, scenario.map.shape_params);
    } else if (scenario.map.shape == "mountain") {
        surface = GenerateMountainShape(w, h, rng, scenario.map.shape_params);
    } else if (scenario.map.shape == "bowl") {
        surface = GenerateBowlShape(w, h, rng, scenario.map.shape_params);
    } else {
        surface = GenerateFlatShape(w, h, rng, scenario.map.shape_params);
    }

    // Pass 2: Assign materials using rules
    if (!scenario.map.materials.empty()) {
        AssignMaterials(terrain, surface, scenario.map.materials, registry);
    } else {
        // Fallback: basic material assignment
        auto* air = registry.GetMaterial("base:Air");
        auto* dirt = registry.GetMaterial("base:Dirt");
        MaterialID air_id = air ? air->runtime_id : 0;
        MaterialID dirt_id = dirt ? dirt->runtime_id : 0;

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                if (y < surface[x]) terrain.SetMaterial(x, y, air_id);
                else terrain.SetMaterial(x, y, dirt_id);
            }
        }
    }

    // Pass 3: Apply features
    ApplyFeatures(terrain, surface, scenario.map.features, registry, rng);

    std::printf("Generated %s map: %dx%d (seed %u)\n", scenario.map.shape.c_str(), w, h, seed);
    return terrain;
}
