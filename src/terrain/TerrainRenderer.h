#pragma once

#include "terrain/Terrain.h"
#include "core/Types.h"
#include "package/MaterialDef.h"
#include <SDL2/SDL.h>
#include <vector>

class Camera;
class DefinitionRegistry;

class TerrainRenderer {
public:
    static constexpr int CHUNK_SIZE = 64;

    TerrainRenderer(SDL_Renderer* renderer, const Terrain& terrain, const DefinitionRegistry* registry = nullptr);
    ~TerrainRenderer();

    TerrainRenderer(const TerrainRenderer&) = delete;
    TerrainRenderer& operator=(const TerrainRenderer&) = delete;

    // Rebuild all chunks
    void FullRebuild();

    // Mark a region as dirty — affected chunks will be rebuilt on next render
    void UpdateRegion(int rx, int ry, int rw, int rh);

    // Render visible chunks
    void Render(SDL_Renderer* renderer, const Camera& camera);

    // Render debug overlay: chunk borders (green) + solid-cell overlay (red)
    void RenderDebugOverlay(SDL_Renderer* renderer, const Camera& camera);

    Color GetMaterialColor(MaterialID id) const;
    Color GetBackgroundColor(BackgroundID id) const;
    Color GetCellColor(const Cell& cell) const;

private:
    struct Chunk {
        SDL_Texture* texture = nullptr;
        SDL_Texture* debug_texture = nullptr;
        bool dirty = true;
        bool debug_dirty = true;
    };

    void RebuildChunk(SDL_Renderer* renderer, int cx, int cy);
    void RebuildDebugChunk(SDL_Renderer* renderer, int cx, int cy);
    int ChunkIndex(int cx, int cy) const { return cy * chunks_x_ + cx; }
    uint32_t ColorToPixel(Color c) const;

    const Terrain& terrain_;
    const DefinitionRegistry* registry_;
    SDL_Renderer* renderer_;  // keep ref for lazy chunk rebuild
    int width_, height_;
    int chunks_x_, chunks_y_;

    std::vector<Chunk> chunks_;

    // Pre-built color LUTs for fast cell coloring (no registry lookup in hot loop)
    std::vector<uint32_t> fg_color_lut_;   // indexed by MaterialID
    std::vector<uint32_t> bg_color_lut_;   // indexed by BackgroundID
    std::vector<bool> is_air_lut_;         // true if material state == None
    std::vector<bool> bg_transparent_lut_; // true if background is transparent/sky
};
