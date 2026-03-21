#pragma once
#include "terrain/TerrainFacade.h"
#include "terrain/TerrainRenderer.h"
#include "terrain/TerrainGen.h"
#include "camera/GameCamera.h"
#include "entity/Character.h"
#include "input/InputManager.h"

class Game {
public:
    Game(int screen_w, int screen_h);

    void update(float dt);
    void draw() const;

private:
    int m_screen_w, m_screen_h;

    InputManager     m_input;
    TerrainFacade    m_terrain;
    TerrainRenderer  m_terrain_renderer;
    GameCamera       m_camera;
    Character        m_character;
};
