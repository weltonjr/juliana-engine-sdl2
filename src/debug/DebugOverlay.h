#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../core/Math.h"

class TerrainFacade;
class Character;
class GameCamera;

// Toggled with F1. Draws:
//   - Semi-transparent info panel (char state, velocity, position, camera)
//   - Chunk grid lines over the viewport
//   - Dirty-chunk highlight (visual=red, collision=yellow borders)
class DebugOverlay {
public:
    bool enabled = false;

    void toggle() { enabled = !enabled; }

    void draw(const TerrainFacade& terrain,
              const Character&     character,
              const GameCamera&    camera,
              int screen_w, int screen_h,
              SDL_Renderer* renderer,
              TTF_Font*     font) const;

private:
    void draw_info_panel(const Character& character,
                         const GameCamera& camera,
                         SDL_Renderer* renderer,
                         TTF_Font*     font) const;

    void draw_chunk_grid(const TerrainFacade& terrain,
                         Vector2 cam_offset,
                         int screen_w, int screen_h,
                         SDL_Renderer* renderer,
                         TTF_Font*     font) const;
};
