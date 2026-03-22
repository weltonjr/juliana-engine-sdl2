#include "TerrainRenderer.h"
#include "MaterialColors.h"
#include "../core/Types.h"

// ── public ─────────────────────────────────────────────────────────────────

void TerrainRenderer::bake_dirty_chunks(TerrainFacade& terrain, SDL_Renderer* renderer) {
    // Iterate all chunks — bake any that are dirty.
    // With only 32 chunks total this is fast even when all are dirty at startup.
    for (int cy = 0; cy < terrain.chunks_y(); cy++)
        for (int cx = 0; cx < terrain.chunks_x(); cx++) {
            TerrainChunk& chunk = terrain.get_chunk(cx, cy);
            if (chunk.dirty_visual)
                bake_chunk(chunk, terrain, renderer);
        }
}

void TerrainRenderer::draw(const TerrainFacade& terrain,
                            Vector2 camera_offset,
                            int screen_w, int screen_h,
                            SDL_Renderer* renderer) const
{
    // Visible chunk range
    int cx0 = (int)(camera_offset.x / CHUNK_PX);
    int cy0 = (int)(camera_offset.y / CHUNK_PX);
    int cx1 = (int)((camera_offset.x + screen_w)  / CHUNK_PX) + 1;
    int cy1 = (int)((camera_offset.y + screen_h) / CHUNK_PX) + 1;

    cx0 = cx0 < 0 ? 0 : cx0;
    cy0 = cy0 < 0 ? 0 : cy0;
    cx1 = cx1 > terrain.chunks_x() ? terrain.chunks_x() : cx1;
    cy1 = cy1 > terrain.chunks_y() ? terrain.chunks_y() : cy1;

    for (int cy = cy0; cy < cy1; cy++) {
        for (int cx = cx0; cx < cx1; cx++) {
            const TerrainChunk& chunk = terrain.get_chunk(cx, cy);
            if (!chunk.tex_valid) continue;

            float sx = (float)(cx * CHUNK_PX) - camera_offset.x;
            float sy = (float)(cy * CHUNK_PX) - camera_offset.y;

            // src: CHUNK_CELLS × CHUNK_CELLS texels (one per cell)
            // dst: CHUNK_PX × CHUNK_PX screen pixels  (CELL_SIZE scale = 4×)
            SDL_Rect  src = {0, 0, CHUNK_CELLS, CHUNK_CELLS};
            SDL_FRect dst = {sx, sy, (float)CHUNK_PX, (float)CHUNK_PX};
            SDL_RenderCopyF(renderer, chunk.tex, &src, &dst);
        }
    }
}

// ── private ────────────────────────────────────────────────────────────────

void TerrainRenderer::bake_chunk(TerrainChunk& chunk,
                                  const TerrainFacade& terrain,
                                  SDL_Renderer* renderer) const
{
    // Build a CHUNK_CELLS × CHUNK_CELLS CPU surface (1 px = 1 cell)
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(
        0, CHUNK_CELLS, CHUNK_CELLS, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_FillRect(surf, nullptr, 0); // transparent

    SDL_LockSurface(surf);
    Uint32* pixels  = (Uint32*)surf->pixels;
    int base_cx = chunk.chunk_x * CHUNK_CELLS;
    int base_cy = chunk.chunk_y * CHUNK_CELLS;

    for (int ly = 0; ly < CHUNK_CELLS; ly++) {
        for (int lx = 0; lx < CHUNK_CELLS; lx++) {
            int gcx = base_cx + lx;
            int gcy = base_cy + ly;

            MaterialID mat = terrain.get_material(gcx, gcy);
            if (mat == MaterialID::EMPTY || mat == MaterialID::AIR) continue;

            Color c = material_cell_color(mat, gcx, gcy);
            pixels[ly * CHUNK_CELLS + lx] =
                SDL_MapRGBA(surf->format, c.r, c.g, c.b, c.a);
        }
    }
    SDL_UnlockSurface(surf);

    // Upload to GPU
    if (chunk.tex_valid) SDL_DestroyTexture(chunk.tex);
    chunk.tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_SetTextureScaleMode(chunk.tex, SDL_ScaleModeNearest); // sharp pixels
    SDL_FreeSurface(surf);
    chunk.tex_valid    = true;
    chunk.dirty_visual = false;
}
