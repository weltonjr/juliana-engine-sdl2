#pragma once

#include "terrain/Terrain.h"
#include "package/DefinitionRegistry.h"
#include <cstdint>

class MapGenerator {
public:
    static Terrain GenerateFlat(int width, int height, uint32_t seed, const DefinitionRegistry& registry);
};
