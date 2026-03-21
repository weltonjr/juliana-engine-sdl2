#pragma once
#include "TerrainFacade.h"

// Generates the test map in-place on the given TerrainFacade.
// Layout: sky (top 20%), dirt (next 50%), rock (bottom 30%)
// Gold ore veins scattered using simple noise.
void terrain_generate(TerrainFacade& terrain);
