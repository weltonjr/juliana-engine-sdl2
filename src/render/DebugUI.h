#pragma once

#include <string>

class Camera;
class Terrain;
class DefinitionRegistry;
struct Entity;

// Debug overlay rendered with Dear ImGui (FPS, mouse-world cell, player state).
//
// Update() collects state from sim systems each tick; DrawImGui() emits ImGui
// calls and is wired into ImGuiBackend's per-frame callback list. No SDL_ttf,
// no texture cache, no manual layout — ImGui handles all of that.
class DebugUI {
public:
    DebugUI() = default;
    ~DebugUI() = default;

    DebugUI(const DebugUI&) = delete;
    DebugUI& operator=(const DebugUI&) = delete;

    void Update(int mouse_x, int mouse_y, const Camera& camera,
                const Terrain& terrain, const DefinitionRegistry& registry,
                const Entity* player);

    // Emits the ImGui window. Call between ImGui::NewFrame() and ImGui::Render().
    void DrawImGui();

private:
    std::string material_name_;
    std::string material_state_;
    int world_mouse_x_ = 0;
    int world_mouse_y_ = 0;

    std::string player_info_;
    std::string action_info_;
};
