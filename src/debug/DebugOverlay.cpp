#include "DebugOverlay.h"
#include "../terrain/TerrainFacade.h"
#include "../terrain/TerrainChunk.h"
#include "../entity/Character.h"
#include "../camera/GameCamera.h"
#include "../core/Types.h"
#include <cstdio>

static constexpr int FONT_SIZE  = 14;
static constexpr int LINE_H     = 17;

// ── public ─────────────────────────────────────────────────────────────────

void DebugOverlay::draw(const TerrainFacade& terrain,
                        const Character&     character,
                        const GameCamera&    camera,
                        int screen_w, int screen_h) const
{
    if (!enabled) return;

    Vector2 cam_offset = camera.offset();

    draw_chunk_grid(terrain, cam_offset, screen_w, screen_h);
    draw_info_panel(character, camera);
}

// ── private ────────────────────────────────────────────────────────────────

void DebugOverlay::draw_info_panel(const Character&  character,
                                   const GameCamera& camera) const
{
    // Semi-transparent background
    constexpr int PW = 300, PH = 160;
    DrawRectangle(6, 6, PW, PH, {0, 0, 0, 160});
    DrawRectangleLines(6, 6, PW, PH, {255, 255, 255, 80});

    char buf[128];
    int  y = 12;
    auto line = [&](const char* txt, Color col = WHITE) {
        DrawText(txt, 12, y, FONT_SIZE, col);
        y += LINE_H;
    };

    line("── DEBUG (F1 to hide) ──", GRAY);

    // FPS
    snprintf(buf, sizeof(buf), "FPS: %d", GetFPS());
    line(buf, GetFPS() >= 55 ? GREEN : RED);

    // Character
    Vector2 pos = character.position();
    Vector2 cen = character.center();
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
                                   int screen_w, int screen_h) const
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
                DrawRectangle(sx, sy, CHUNK_PX, CHUNK_PX, {255, 80, 80, 30});
            } else if (chunk.dirty_visual) {
                DrawRectangle(sx, sy, CHUNK_PX, CHUNK_PX, {255, 200, 0, 25});
            } else if (chunk.dirty_collision) {
                DrawRectangle(sx, sy, CHUNK_PX, CHUNK_PX, {0, 200, 255, 25});
            }

            // Chunk border
            Color border_col = chunk.dirty_visual ? RED : (chunk.dirty_collision ? YELLOW : Color{255, 255, 255, 50});
            DrawRectangleLines(sx, sy, CHUNK_PX, CHUNK_PX, border_col);

            // Chunk coordinates label (top-left corner)
            char label[16];
            snprintf(label, sizeof(label), "%d,%d", cx, cy);
            DrawText(label, sx + 3, sy + 3, 10, {255, 255, 255, 120});
        }
    }
}
