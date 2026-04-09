#pragma once

#include "core/Window.h"
#include "core/GameLoop.h"
#include "terrain/Terrain.h"
#include "terrain/TerrainRenderer.h"
#include "terrain/MapGenerator.h"
#include "render/Camera.h"
#include "input/InputManager.h"
#include "input/InputSystem.h"
#include "package/DefinitionRegistry.h"
#include "package/PackageLoader.h"
#include "entity/EntityManager.h"
#include "entity/ActionMap.h"
#include "physics/PhysicsSystem.h"
#include "terrain/TerrainSimulator.h"
#include "scenario/ScenarioDef.h"
#include "scenario/ScenarioLoader.h"
#include "render/DebugUI.h"
#include "game/GameDef.h"
#include "ui/UISystem.h"
#include "scripting/LuaState.h"
#include "core/LogConsole.h"
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>

class Engine {
public:
    Engine();
    ~Engine();

    // Load game definition from game_path, initialize all systems, and run the startup Lua script.
    void Init(const std::string& game_path = "packages/mapeditor/game");
    void Run();

    void SimTick(double dt);
    void Render(double alpha);

    bool ShouldQuit() const;
    void SetWindowTitle(const char* title);
    void RequestQuit() { quit_requested_ = true; }

    // Generic terrain API — generate terrain from a ScenarioDef (no entities/physics)
    void GenerateTerrain(const ScenarioDef& scenario);
    void UnloadTerrain();

    // Per-tick Lua callback — called every SimTick regardless of sim state
    void SetTickCallback(std::function<void(double)> fn) { tick_callback_ = std::move(fn); }

    // Raw input access for Lua bindings
    const InputSystem&  GetRaw()   const;
    const InputManager& GetInput() const;

    // Terrain state queries
    bool IsTerrainLoaded()  const { return terrain_renderer_ != nullptr; }
    int  GetTerrainWidth()  const { return terrain_ ? terrain_->GetWidth()  : 0; }
    int  GetTerrainHeight() const { return terrain_ ? terrain_->GetHeight() : 0; }

    // Camera accessors for Lua
    float GetCameraX()    const { return cameras_.empty() ? 0.f : cameras_[0]->GetX(); }
    float GetCameraY()    const { return cameras_.empty() ? 0.f : cameras_[0]->GetY(); }
    float GetCameraZoom() const { return cameras_.empty() ? 1.f : cameras_[0]->GetScale(); }
    void  SetCameraPosition(float x, float y) {
        if (!cameras_.empty()) {
            cameras_[0]->SetPosition(x, y);
            if (terrain_) cameras_[0]->ClampToBounds(terrain_->GetWidth(), terrain_->GetHeight());
        }
    }
    void MoveCamera(float dx, float dy) {
        if (!cameras_.empty()) {
            cameras_[0]->Move(dx, dy);
            if (terrain_) cameras_[0]->ClampToBounds(terrain_->GetWidth(), terrain_->GetHeight());
        }
    }
    void SetCameraZoom(float s) {
        if (!cameras_.empty()) cameras_[0]->SetScale(s);
    }

    const DefinitionRegistry& GetRegistry() const { return registry_; }

private:
    // --- Gameplay helpers (only called when sim_running_) ---
    void UpdatePlayerControl(Entity& entity, double dt);
    void UpdateCameraFollow(Camera& cam, const Entity& target);
    void AdvanceActions(double dt);
    void RenderEntities(SDL_Renderer* renderer, double alpha);

    // ── Simulation bootstrap ────────────────────────────────────────────────────
    // Loads a scenario, generates terrain, and spawns initial entities.
    // Called once sim_running_ transitions to true (in future: from Lua).
    void InitSimulation(const std::string& scenario_path);

    static constexpr int WINDOW_WIDTH  = 1280;
    static constexpr int WINDOW_HEIGHT = 720;
    static constexpr int TERRAIN_WIDTH  = 2048;
    static constexpr int TERRAIN_HEIGHT = 512;

    // ── Core (always present) ───────────────────────────────────────────────────
    GameDef game_def_;
    DefinitionRegistry registry_;

    std::unique_ptr<Window>       window_;
    std::unique_ptr<GameLoop>     game_loop_;
    std::unique_ptr<InputManager> input_;
    std::unique_ptr<LuaState>     lua_state_;
    std::unique_ptr<UISystem>     ui_system_;

    std::vector<std::unique_ptr<Camera>> cameras_;

    std::unique_ptr<LogConsole> log_console_;
    bool log_console_visible_ = false;
    bool quit_requested_ = false;

    std::function<void(double)> tick_callback_;

    // ── Simulation (null until sim_running_ = true) ─────────────────────────────
    bool sim_running_ = false;

    std::unique_ptr<Terrain>          terrain_;
    std::unique_ptr<TerrainRenderer>  terrain_renderer_;
    std::unique_ptr<EntityManager>    entity_manager_;
    std::unique_ptr<PhysicsSystem>    physics_;
    std::unique_ptr<TerrainSimulator> terrain_sim_;
    std::unique_ptr<DebugUI>          debug_ui_;

    std::unordered_map<std::string, ActionMap> action_maps_;
    std::vector<EntityID> controllable_entities_;
    int                   active_char_index_ = 0;
};
