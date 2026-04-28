#include "render/DebugUI.h"
#include "render/Camera.h"
#include "terrain/Terrain.h"
#include "package/DefinitionRegistry.h"
#include "package/MaterialDef.h"
#include "entity/Entity.h"

#include <imgui.h>
#include <cstdio>

void DebugUI::Update(int mouse_x, int mouse_y, const Camera& camera,
                     const Terrain& terrain, const DefinitionRegistry& registry,
                     const Entity* player)
{
    float wx, wy;
    camera.ScreenToWorld(mouse_x, mouse_y, wx, wy);
    world_mouse_x_ = static_cast<int>(wx);
    world_mouse_y_ = static_cast<int>(wy);

    if (terrain.InBounds(world_mouse_x_, world_mouse_y_)) {
        Cell cell = terrain.GetCell(world_mouse_x_, world_mouse_y_);
        auto* mat = registry.GetMaterialByRuntimeID(cell.material_id);
        if (mat) {
            material_name_ = mat->name;
            switch (mat->state) {
                case MaterialState::None:   material_state_ = "none"; break;
                case MaterialState::Solid:  material_state_ = "solid"; break;
                case MaterialState::Powder: material_state_ = "powder"; break;
                case MaterialState::Liquid: material_state_ = "liquid"; break;
                case MaterialState::Gas:    material_state_ = "gas"; break;
            }
        } else {
            material_name_ = "Unknown";
            material_state_ = "?";
        }
    } else {
        material_name_ = "Out of bounds";
        material_state_ = "";
    }

    if (player) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "pos: %.1f, %.1f  vel: %.1f, %.1f  ground: %s",
            player->pos_x, player->pos_y,
            player->vel_x, player->vel_y,
            player->on_ground ? "yes" : "no");
        player_info_ = buf;
        action_info_ = "action: " + player->current_action;
    }
}

void DebugUI::DrawImGui() {
    ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug",
                      nullptr,
                      ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("cursor: %d, %d  [%s] (%s)",
                world_mouse_x_, world_mouse_y_,
                material_name_.c_str(), material_state_.c_str());

    if (!player_info_.empty()) {
        ImGui::Separator();
        ImGui::TextUnformatted(player_info_.c_str());
        ImGui::TextUnformatted(action_info_.c_str());
    }

    ImGui::End();
}
