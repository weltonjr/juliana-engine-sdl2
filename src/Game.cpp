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
    // Snap camera to center of map
    m_camera.snap_to({m_cam_target_x, m_cam_target_y});
}

void Game::update(float dt) {
    // Temporary camera pan with arrow keys (removed once character exists)
    float pan_speed = 400.0f;
    if (IsKeyDown(KEY_RIGHT)) m_cam_target_x += pan_speed * dt;
    if (IsKeyDown(KEY_LEFT))  m_cam_target_x -= pan_speed * dt;
    if (IsKeyDown(KEY_DOWN))  m_cam_target_y += pan_speed * dt;
    if (IsKeyDown(KEY_UP))    m_cam_target_y -= pan_speed * dt;

    m_camera.follow({m_cam_target_x, m_cam_target_y}, dt);
}

void Game::draw() const {
    ClearBackground({100, 149, 237, 255}); // Sky blue background

    Vector2 offset = m_camera.offset();
    m_terrain_renderer.draw(m_terrain, offset, m_screen_w, m_screen_h);

    // HUD
    DrawFPS(10, 10);
    DrawText("Aeterium v0.1 - Arrow keys to pan", 10, 30, 16, WHITE);
}
