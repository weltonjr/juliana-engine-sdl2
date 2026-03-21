#pragma once
#include "terrain/TerrainFacade.h"
#include "terrain/TerrainRenderer.h"
#include "terrain/TerrainGen.h"
#include "camera/GameCamera.h"

class Game {
public:
    Game(int screen_w, int screen_h);

    void update(float dt);
    void draw() const;

private:
    int m_screen_w, m_screen_h;

    TerrainFacade    m_terrain;
    TerrainRenderer  m_terrain_renderer;
    GameCamera       m_camera;

    // Camera pan with arrow keys (temporary, until character exists)
    float m_cam_target_x = MAP_PX_W * 0.5f;
    float m_cam_target_y = MAP_PX_H * 0.5f;
};
