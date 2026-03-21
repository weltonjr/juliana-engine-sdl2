#include "Game.h"
#include "raylib.h"
#include "core/Types.h"

Game::Game(int screen_w, int screen_h)
    : m_screen_w(screen_w),
      m_screen_h(screen_h),
      m_terrain(MAP_CELLS_W, MAP_CELLS_H),
      m_camera(screen_w, screen_h, MAP_PX_W, MAP_PX_H)
{
    terrain_generate(m_terrain);
}

Game::~Game() {
    for (int cy = 0; cy < m_terrain.chunks_y(); cy++)
        for (int cx = 0; cx < m_terrain.chunks_x(); cx++) {
            auto& chunk = m_terrain.get_chunk(cx, cy);
            if (chunk.tex_valid) UnloadTexture(chunk.tex);
        }
}

void Game::update(float dt) {
    // Toggle debug overlay
    if (IsKeyPressed(KEY_F1)) m_debug.toggle();

    // Toggle free camera
    if (IsKeyPressed(KEY_F2)) {
        m_free_cam = !m_free_cam;
        if (m_free_cam) {
            Vector2 off   = m_camera.offset();
            m_free_cam_x  = off.x + m_screen_w * 0.5f;
            m_free_cam_y  = off.y + m_screen_h * 0.5f;
        }
    }

    m_input.poll();

    if (!m_free_cam) {
        m_character.update(dt, m_input, m_terrain);
        m_camera.follow(m_character.center(), dt);
    } else {
        update_free_camera(dt);
    }

    // Rebuild GPU textures for any dirty chunks (runs after dig/explode mutations)
    m_terrain_renderer.bake_dirty_chunks(m_terrain);
}

void Game::update_free_camera(float dt) {
    if (IsKeyDown(KEY_RIGHT)) m_free_cam_x += FREE_CAM_SPEED * dt;
    if (IsKeyDown(KEY_LEFT))  m_free_cam_x -= FREE_CAM_SPEED * dt;
    if (IsKeyDown(KEY_DOWN))  m_free_cam_y += FREE_CAM_SPEED * dt;
    if (IsKeyDown(KEY_UP))    m_free_cam_y -= FREE_CAM_SPEED * dt;
    m_camera.follow({m_free_cam_x, m_free_cam_y}, dt);
}

void Game::draw() const {
    // Sky gradient: light azure at top → deeper blue at the horizon
    ClearBackground({148, 210, 240, 255}); // fallback
    DrawRectangleGradientV(0, 0, m_screen_w, m_screen_h,
        {148, 210, 240, 255},   // light sky blue (top)
        {72,  130, 200, 255});  // deeper horizon blue (bottom)

    Vector2 offset = m_camera.offset();
    m_terrain_renderer.draw(m_terrain, offset, m_screen_w, m_screen_h);
    m_character.draw(offset);

    // Debug overlay (drawn on top of everything)
    m_debug.draw(m_terrain, m_character, m_camera, m_screen_w, m_screen_h);

    // Minimal HUD (always visible)
    if (!m_debug.enabled) {
        DrawFPS(10, 10);
        const char* mode = m_free_cam ? " [FREE CAM]" : "";
        char hud[64];
        snprintf(hud, sizeof(hud), "A/D: move  SPACE: jump  C: dig%s", mode);
        DrawText(hud, 10, 30, 13, {255, 255, 255, 180});
        DrawText(char_state_name(m_character.state()), 10, 48, 13, YELLOW);
    }
}
