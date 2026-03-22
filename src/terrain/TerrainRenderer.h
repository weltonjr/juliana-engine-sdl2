#pragma once
#include <SDL2/SDL.h>
#include "TerrainFacade.h"
#include "TerrainChunk.h"
#include "../core/Math.h"

// Terrain renderer using per-chunk GPU textures.
//
// Each chunk is a CHUNK_CELLS × CHUNK_CELLS SDL_Texture* baked from the bitmap
// (one texel = one cell). It is drawn at CHUNK_PX × CHUNK_PX screen pixels
// (CELL_SIZE scale) with SDL_ScaleModeNearest for sharp pixel look.
//
// Separation of concerns:
//   bake_dirty_chunks() — called in Game::update(), mutates chunk state
//   draw()              — called in Game::draw(), const, just blits textures
class TerrainRenderer {
public:
    // Rebuilds GPU textures for all dirty chunks. Call once per update tick.
    void bake_dirty_chunks(TerrainFacade& terrain, SDL_Renderer* renderer);

    // Blits visible chunk textures. const — no state mutation.
    void draw(const TerrainFacade& terrain, Vector2 camera_offset,
              int screen_w, int screen_h, SDL_Renderer* renderer) const;

private:
    void bake_chunk(TerrainChunk& chunk, const TerrainFacade& terrain,
                    SDL_Renderer* renderer) const;
};
