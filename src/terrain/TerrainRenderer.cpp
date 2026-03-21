#include "TerrainRenderer.h"
#include "MaterialColors.h"
#include "../core/Types.h"

// ── public ─────────────────────────────────────────────────────────────────

void TerrainRenderer::bake_dirty_chunks(TerrainFacade& terrain) {
    // Iterate all chunks — bake any that are dirty.
    // With only 32 chunks total this is fast even when all are dirty at startup.
    for (int cy = 0; cy < terrain.chunks_y(); cy++)
        for (int cx = 0; cx < terrain.chunks_x(); cx++) {
            TerrainChunk& chunk = terrain.get_chunk(cx, cy);
            if (chunk.dirty_visual)
                bake_chunk(chunk, terrain);
        }
}

void TerrainRenderer::draw(const TerrainFacade& terrain,
                            Vector2 camera_offset,
                            int screen_w, int screen_h) const
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
            // No Y-flip needed — Texture2D (not RenderTexture2D) is top-down.
            Rectangle src  = { 0.0f, 0.0f, (float)CHUNK_CELLS, (float)CHUNK_CELLS };
            Rectangle dest = { sx,   sy,   (float)CHUNK_PX,    (float)CHUNK_PX    };
            DrawTexturePro(chunk.tex, src, dest, {0.0f, 0.0f}, 0.0f, WHITE);
        }
    }
}

// ── private ────────────────────────────────────────────────────────────────

void TerrainRenderer::bake_chunk(TerrainChunk& chunk,
                                  const TerrainFacade& terrain) const
{
    // Build a CHUNK_CELLS × CHUNK_CELLS CPU image (1 px = 1 cell)
    Image img = GenImageColor(CHUNK_CELLS, CHUNK_CELLS, {0, 0, 0, 0});

    int base_cx = chunk.chunk_x * CHUNK_CELLS;
    int base_cy = chunk.chunk_y * CHUNK_CELLS;

    for (int ly = 0; ly < CHUNK_CELLS; ly++) {
        for (int lx = 0; lx < CHUNK_CELLS; lx++) {
            int gcx = base_cx + lx;
            int gcy = base_cy + ly;

            MaterialID mat = terrain.get_material(gcx, gcy);
            if (mat == MaterialID::EMPTY || mat == MaterialID::AIR) continue;

            ImageDrawPixel(&img, lx, ly, material_cell_color(mat, gcx, gcy));
        }
    }

    // Upload to GPU
    if (chunk.tex_valid) UnloadTexture(chunk.tex);
    chunk.tex = LoadTextureFromImage(img);
    SetTextureFilter(chunk.tex, TEXTURE_FILTER_POINT); // sharp pixels, no blur
    chunk.tex_valid    = true;
    chunk.dirty_visual = false;

    UnloadImage(img);
}
