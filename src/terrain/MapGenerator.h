#pragma once

#include "terrain/Terrain.h"
#include "package/DefinitionRegistry.h"
#include "scenario/ScenarioDef.h"
#include <cstdint>
#include <random>
#include <vector>

struct SpawnPosition {
    int x, y;
};

class MapGenerator {
public:
    // seed_used_out — if non-null, receives the actual seed used (may differ from
    // scenario.map.seed when seed==0, in which case a random seed is generated).
    static Terrain GenerateFromScenario(const ScenarioDef& scenario, const DefinitionRegistry& registry,
                                        uint32_t* seed_used_out = nullptr);

    // Find spawn positions for player slots
    static std::vector<SpawnPosition> FindSpawnPositions(
        const Terrain& terrain, const DefinitionRegistry& registry,
        const std::vector<PlayerSlot>& slots);

    // Helper: get surface Y at column (first non-air from top)
    static int FindSurfaceY(const Terrain& terrain, int x, const DefinitionRegistry& registry);

private:
    // Shape generators — output surface heightmap
    static std::vector<int> GenerateFlatShape(int width, int height, std::mt19937& rng, const MapShapeParams& params);
    static std::vector<int> GenerateIslandShape(int width, int height, std::mt19937& rng, const MapShapeParams& params);
    static std::vector<int> GenerateMountainShape(int width, int height, std::mt19937& rng, const MapShapeParams& params);
    static std::vector<int> GenerateBowlShape(int width, int height, std::mt19937& rng, const MapShapeParams& params);

    // Material assignment pass
    static void AssignMaterials(Terrain& terrain, const std::vector<int>& surface,
                                const std::vector<MaterialRule>& rules,
                                const DefinitionRegistry& registry,
                                int sea_level_y = -1);

    // Feature passes
    static void ApplyFeatures(Terrain& terrain, const std::vector<int>& surface,
                              const std::vector<FeatureConfig>& features,
                              const DefinitionRegistry& registry, std::mt19937& rng);

    static void GenerateCaves(Terrain& terrain, const std::vector<int>& surface,
                              const FeatureConfig& config,
                              const DefinitionRegistry& registry, std::mt19937& rng);
    static void GenerateOreVeins(Terrain& terrain, const std::vector<int>& surface,
                                  const FeatureConfig& config,
                                  const DefinitionRegistry& registry, std::mt19937& rng);
    static void GenerateLakes(Terrain& terrain, const std::vector<int>& surface,
                              const FeatureConfig& config,
                              const DefinitionRegistry& registry, std::mt19937& rng);
};
