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

void Game::update(float dt) {
    m_input.poll();
    m_character.update(dt, m_input, m_terrain);
    m_camera.follow(m_character.center(), dt);
}

void Game::draw() const {
    ClearBackground({100, 149, 237, 255}); // Sky blue

    Vector2 offset = m_camera.offset();
    m_terrain_renderer.draw(m_terrain, offset, m_screen_w, m_screen_h);
    m_character.draw(offset);

    // HUD
    DrawFPS(10, 10);
    DrawText("A/D: move  SPACE: jump  C: dig", 10, 30, 14, WHITE);
    DrawText(char_state_name(m_character.state()), 10, 48, 14, YELLOW);
}
