#include "DebugOverlay.h"
#include "../terrain/TerrainFacade.h"
#include "../terrain/TerrainChunk.h"
#include "../entity/Character.h"
#include "../entity/CharacterFSM.h"
#include "../camera/GameCamera.h"
#include "../core/Types.h"
#include "../core/Color.h"
#include <cstdio>

static constexpr int LINE_H = 17;

// ── SDL2 drawing helpers ────────────────────────────────────────────────────

static void draw_rect_filled(SDL_Renderer* r, int x, int y, int w, int h,
                              uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void draw_rect_lines(SDL_Renderer* r, int x, int y, int w, int h,
                             uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(r, &rect);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

// Render a line of text; does nothing if font is null
static void draw_text(SDL_Renderer* r, TTF_Font* font, const char* text,
                      int x, int y, Color col) {
    if (!font || !text || !*text) return;
    SDL_Color sc = {col.r, col.g, col.b, col.a};
    SDL_Surface* surf = TTF_RenderText_Blended(font, text, sc);
    if (!surf) return;
    int tw, th;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_FreeSurface(surf);
    if (!tex) return;
    SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
    SDL_FRect dst = {(float)x, (float)y, (float)tw, (float)th};
    SDL_RenderCopyF(r, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

// ── public ─────────────────────────────────────────────────────────────────

void DebugOverlay::draw(const TerrainFacade& terrain,
                        const Character&     character,
                        const GameCamera&    camera,
                        int screen_w, int screen_h,
                        SDL_Renderer* renderer,
                        TTF_Font*     font) const
{
    if (!enabled) return;

    Vector2 cam_offset = camera.offset();

    draw_chunk_grid(terrain, cam_offset, screen_w, screen_h, renderer, font);
    draw_info_panel(character, camera, renderer, font);
}

// ── private ────────────────────────────────────────────────────────────────

void DebugOverlay::draw_info_panel(const Character&  character,
                                   const GameCamera& camera,
                                   SDL_Renderer* renderer,
                                   TTF_Font*     font) const
{
    // Semi-transparent background
    constexpr int PW = 300, PH = 160;
    draw_rect_filled(renderer, 6, 6, PW, PH, 0, 0, 0, 160);
    draw_rect_lines (renderer, 6, 6, PW, PH, 255, 255, 255, 80);

    char buf[128];
    int  y = 12;
    auto line = [&](const char* txt, Color col = WHITE) {
        draw_text(renderer, font, txt, 12, y, col);
        y += LINE_H;
    };

    line("-- DEBUG (F1 to hide) --", GRAY);

    // FPS via SDL ticks (simple estimate)
    static Uint64 fps_prev  = 0;
    static int    fps_value = 0;
    Uint64 now_ticks = SDL_GetTicks64();
    if (fps_prev > 0 && now_ticks > fps_prev) {
        fps_value = (int)(1000 / (now_ticks - fps_prev));
    }
    fps_prev = now_ticks;
    snprintf(buf, sizeof(buf), "FPS: %d", fps_value);
    line(buf, fps_value >= 55 ? GREEN : RED);

    // Character
    Vector2 pos = character.position();
    snprintf(buf, sizeof(buf), "State: %s", char_state_name(character.state()));
    line(buf, YELLOW);

    snprintf(buf, sizeof(buf), "Pos:   %.1f, %.1f", pos.x, pos.y);
    line(buf);

    // Camera
    Vector2 off = camera.offset();
    snprintf(buf, sizeof(buf), "Cam:   %.1f, %.1f", off.x, off.y);
    line(buf, SKYBLUE);

    // Map info
    snprintf(buf, sizeof(buf), "Map:   %dx%d cells  (%dx%d chunks)",
             MAP_CELLS_W, MAP_CELLS_H, CHUNKS_X, CHUNKS_Y);
    line(buf, LIGHTGRAY);

    line("[F1] overlay  [F2] free cam", GRAY);
}

void DebugOverlay::draw_chunk_grid(const TerrainFacade& terrain,
                                   Vector2 cam_offset,
                                   int screen_w, int screen_h,
                                   SDL_Renderer* renderer,
                                   TTF_Font*     font) const
{
    int cx_start = (int)(cam_offset.x / CHUNK_PX);
    int cy_start = (int)(cam_offset.y / CHUNK_PX);
    int cx_end   = (int)((cam_offset.x + screen_w)  / CHUNK_PX) + 1;
    int cy_end   = (int)((cam_offset.y + screen_h) / CHUNK_PX) + 1;

    cx_start = cx_start < 0 ? 0 : cx_start;
    cy_start = cy_start < 0 ? 0 : cy_start;
    cx_end   = cx_end > terrain.chunks_x() ? terrain.chunks_x() : cx_end;
    cy_end   = cy_end > terrain.chunks_y() ? terrain.chunks_y() : cy_end;

    for (int cy = cy_start; cy < cy_end; cy++) {
        for (int cx = cx_start; cx < cx_end; cx++) {
            const TerrainChunk& chunk = terrain.get_chunk(cx, cy);

            int sx = (int)(cx * CHUNK_PX - cam_offset.x);
            int sy = (int)(cy * CHUNK_PX - cam_offset.y);

            // Dirty highlight: semi-transparent fill
            if (chunk.dirty_visual && chunk.dirty_collision) {
                draw_rect_filled(renderer, sx, sy, CHUNK_PX, CHUNK_PX, 255, 80, 80, 30);
            } else if (chunk.dirty_visual) {
                draw_rect_filled(renderer, sx, sy, CHUNK_PX, CHUNK_PX, 255, 200, 0, 25);
            } else if (chunk.dirty_collision) {
                draw_rect_filled(renderer, sx, sy, CHUNK_PX, CHUNK_PX, 0, 200, 255, 25);
            }

            // Chunk border
            if (chunk.dirty_visual) {
                draw_rect_lines(renderer, sx, sy, CHUNK_PX, CHUNK_PX, 255, 0, 0, 255);
            } else if (chunk.dirty_collision) {
                draw_rect_lines(renderer, sx, sy, CHUNK_PX, CHUNK_PX, 255, 235, 0, 255);
            } else {
                draw_rect_lines(renderer, sx, sy, CHUNK_PX, CHUNK_PX, 255, 255, 255, 50);
            }

            // Chunk coordinates label (top-left corner)
            char label[16];
            snprintf(label, sizeof(label), "%d,%d", cx, cy);
            draw_text(renderer, font, label, sx + 3, sy + 3, {255, 255, 255, 120});
        }
    }
}
