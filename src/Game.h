#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "terrain/TerrainFacade.h"
#include "terrain/TerrainRenderer.h"
#include "terrain/TerrainGen.h"
#include "camera/GameCamera.h"
#include "entity/Character.h"
#include "input/InputManager.h"
#include "debug/DebugOverlay.h"

class Game {
public:
    Game(int screen_w, int screen_h, SDL_Renderer* renderer);
    ~Game(); // unloads chunk GPU textures

    void poll_events();                // processes SDL events, updates input + quit flag
    void update(float dt);
    void draw(SDL_Renderer* renderer) const;

    bool quit_requested() const { return m_quit; }

private:
    void update_free_camera(float dt);
    void draw_text(SDL_Renderer* r, const char* text, int x, int y,
                   uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca) const;

    int m_screen_w, m_screen_h;
    bool m_quit = false;

    SDL_Renderer* m_renderer = nullptr;  // non-owning reference
    TTF_Font*     m_font     = nullptr;

    InputManager     m_input;
    TerrainFacade    m_terrain;
    TerrainRenderer  m_terrain_renderer;
    GameCamera       m_camera;
    Character        m_character;
    DebugOverlay     m_debug;

    // Free-camera mode (F2): arrow keys pan instead of following character
    bool  m_free_cam      = false;
    float m_free_cam_x    = MAP_PX_W * 0.5f;
    float m_free_cam_y    = MAP_PX_H * 0.5f;
    static constexpr float FREE_CAM_SPEED = 500.0f;

    // F1/F2 toggle: track previous frame key state for rising-edge detection
    bool m_f1_prev = false;
    bool m_f2_prev = false;
};
