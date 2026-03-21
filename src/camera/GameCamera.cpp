#include "GameCamera.h"
#include "../core/Math.h"
#include <cmath>

GameCamera::GameCamera(int screen_w, int screen_h, int world_px_w, int world_px_h)
    : m_screen_w(screen_w), m_screen_h(screen_h),
      m_world_w(world_px_w), m_world_h(world_px_h),
      m_x(0), m_y(0)
{}

void GameCamera::follow(Vector2 target, float dt) {
    // Target: center the camera on the target position
    float target_x = target.x - m_screen_w * 0.5f;
    float target_y = target.y - m_screen_h * 0.5f;

    // Exponential lerp: smooth but responsive
    float t = 1.0f - std::pow(0.01f, dt * m_speed);
    m_x += (target_x - m_x) * t;
    m_y += (target_y - m_y) * t;

    clamp();
}

void GameCamera::snap_to(Vector2 target) {
    m_x = target.x - m_screen_w * 0.5f;
    m_y = target.y - m_screen_h * 0.5f;
    clamp();
}

Vector2 GameCamera::offset() const {
    return { m_x, m_y };
}

void GameCamera::clamp() {
    float max_x = (float)(m_world_w  - m_screen_w);
    float max_y = (float)(m_world_h - m_screen_h);
    m_x = clampf(m_x, 0.0f, max_x > 0 ? max_x : 0.0f);
    m_y = clampf(m_y, 0.0f, max_y > 0 ? max_y : 0.0f);
}
