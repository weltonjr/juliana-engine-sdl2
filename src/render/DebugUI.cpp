#include "render/DebugUI.h"
#include "render/Camera.h"
#include "terrain/Terrain.h"
#include "package/DefinitionRegistry.h"
#include "package/MaterialDef.h"
#include "entity/Entity.h"
#include <cstdio>

DebugUI::DebugUI(SDL_Renderer* renderer) {
    if (TTF_Init() < 0) {
        std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return;
    }

    // Try common monospace font paths on macOS
    const char* font_paths[] = {
        "/System/Library/Fonts/SFNSMono.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Monaco.ttf",
        "/System/Library/Fonts/Courier.ttc",
        "/System/Library/Fonts/Supplemental/Courier New.ttf",
        nullptr
    };

    for (int i = 0; font_paths[i]; i++) {
        font_ = TTF_OpenFont(font_paths[i], 14);
        if (font_) break;
    }

    if (!font_) {
        std::fprintf(stderr, "Warning: Could not load any system font for debug UI\n");
    }
}

DebugUI::~DebugUI() {
    if (font_) TTF_CloseFont(font_);
    TTF_Quit();
}

void DebugUI::Update(int mouse_x, int mouse_y, const Camera& camera,
                     const Terrain& terrain, const DefinitionRegistry& registry,
                     const Entity* player)
{
    // Convert screen mouse to world coordinates
    float wx, wy;
    camera.ScreenToWorld(mouse_x, mouse_y, wx, wy);
    world_mouse_x_ = static_cast<int>(wx);
    world_mouse_y_ = static_cast<int>(wy);

    // Look up material at mouse position
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
            }
        } else {
            material_name_ = "Unknown";
            material_state_ = "?";
        }
    } else {
        material_name_ = "Out of bounds";
        material_state_ = "";
    }

    // Player info
    if (player) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "pos: %.1f, %.1f  vel: %.1f, %.1f  ground: %s",
            player->pos_x.ToFloat(), player->pos_y.ToFloat(),
            player->vel_x.ToFloat(), player->vel_y.ToFloat(),
            player->on_ground ? "yes" : "no");
        player_info_ = buf;
        action_info_ = "action: " + player->current_action;
    }
}

void DebugUI::RenderText(SDL_Renderer* renderer, const std::string& text, int x, int y) {
    if (!font_ || text.empty()) return;

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(font_, text.c_str(), white);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void DebugUI::Render(SDL_Renderer* renderer) {
    // Semi-transparent background panel
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
    SDL_Rect panel = {4, 4, 360, 80};
    SDL_RenderFillRect(renderer, &panel);

    // Material under cursor
    char buf[256];
    std::snprintf(buf, sizeof(buf), "cursor: %d, %d  [%s] (%s)",
        world_mouse_x_, world_mouse_y_,
        material_name_.c_str(), material_state_.c_str());
    RenderText(renderer, buf, 8, 8);

    // Player info
    RenderText(renderer, player_info_, 8, 28);
    RenderText(renderer, action_info_, 8, 48);
}
