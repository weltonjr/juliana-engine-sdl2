#pragma once
#include "terrain/TerrainFacade.h"
#include "terrain/TerrainRenderer.h"
#include "terrain/TerrainGen.h"
#include "camera/GameCamera.h"
#include "entity/Character.h"
#include "input/InputManager.h"
#include "debug/DebugOverlay.h"

class Game {
public:
    Game(int screen_w, int screen_h);

    void update(float dt);
    void draw() const;

private:
    void update_free_camera(float dt);

    int m_screen_w, m_screen_h;

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
};
