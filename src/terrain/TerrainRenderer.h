#pragma once

#include "terrain/Terrain.h"
#include "core/Types.h"
#include <SDL2/SDL.h>

class Camera;
class DefinitionRegistry;

class TerrainRenderer {
public:
    TerrainRenderer(SDL_Renderer* renderer, const Terrain& terrain, const DefinitionRegistry* registry = nullptr);
    ~TerrainRenderer();

    TerrainRenderer(const TerrainRenderer&) = delete;
    TerrainRenderer& operator=(const TerrainRenderer&) = delete;

    void FullRebuild();
    void Render(SDL_Renderer* renderer, const Camera& camera);

    Color GetMaterialColor(MaterialID id) const;

private:
    SDL_Texture* texture_ = nullptr;
    const Terrain& terrain_;
    const DefinitionRegistry* registry_;
    int width_;
    int height_;
};
