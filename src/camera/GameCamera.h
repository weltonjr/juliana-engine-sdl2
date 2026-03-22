#pragma once
#include "../core/Math.h"

// Smooth-follow camera. Lerps toward target position each frame.
// Clamps to world bounds so the camera never shows outside the map.
class GameCamera {
public:
    GameCamera(int screen_w, int screen_h, int world_px_w, int world_px_h);

    // Call once per physics update to move camera toward target
    void follow(Vector2 target_world_pos, float dt);

    // Snap camera instantly to target (used at spawn)
    void snap_to(Vector2 target_world_pos);

    // Returns the world-space offset to pass to renderers
    // i.e. top-left world position visible on screen
    Vector2 offset() const;

    // Set smoothing speed (default 8.0)
    void set_speed(float speed) { m_speed = speed; }

private:
    void clamp();

    int m_screen_w, m_screen_h;
    int m_world_w,  m_world_h;
    float m_x, m_y;   // current top-left world position
    float m_speed = 8.0f;
};
