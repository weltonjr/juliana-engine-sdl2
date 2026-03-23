#pragma once

#include "core/Window.h"
#include "core/GameLoop.h"
#include "terrain/Terrain.h"
#include "terrain/TerrainRenderer.h"
#include "terrain/MapGenerator.h"
#include "render/Camera.h"
#include "input/InputSystem.h"
#include "package/DefinitionRegistry.h"
#include "package/PackageLoader.h"
#include "entity/EntityManager.h"
#include "entity/ActionMap.h"
#include "physics/PhysicsSystem.h"
#include "render/DebugUI.h"
#include <memory>
#include <unordered_map>

class Engine {
public:
    Engine();
    ~Engine();

    void Init();
    void Run();

    void SimTick(double dt);
    void Render(double alpha);

    bool ShouldQuit() const;
    void SetWindowTitle(const char* title);

    const DefinitionRegistry& GetRegistry() const { return registry_; }

private:
    void UpdatePlayerControl(Entity& entity, double dt);
    void AdvanceActions(double dt);
    void RenderEntities(SDL_Renderer* renderer, double alpha);

    static constexpr int WINDOW_WIDTH = 1280;
    static constexpr int WINDOW_HEIGHT = 720;
    static constexpr int TERRAIN_WIDTH = 2048;
    static constexpr int TERRAIN_HEIGHT = 512;
    static constexpr float CAMERA_SPEED = 300.0f;

    DefinitionRegistry registry_;

    std::unique_ptr<Window> window_;
    std::unique_ptr<GameLoop> game_loop_;
    std::unique_ptr<Terrain> terrain_;
    std::unique_ptr<TerrainRenderer> terrain_renderer_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<InputSystem> input_;
    std::unique_ptr<EntityManager> entity_manager_;
    std::unique_ptr<PhysicsSystem> physics_;

    // Action maps keyed by object qualified_id
    std::unordered_map<std::string, ActionMap> action_maps_;

    std::unique_ptr<DebugUI> debug_ui_;

    EntityID player_entity_id_ = 0;
};
