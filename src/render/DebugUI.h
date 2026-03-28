#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <unordered_map>

class Camera;
class Terrain;
class DefinitionRegistry;
struct Entity;

class DebugUI {
public:
    DebugUI(SDL_Renderer* renderer);
    ~DebugUI();

    DebugUI(const DebugUI&) = delete;
    DebugUI& operator=(const DebugUI&) = delete;

    void Update(int mouse_x, int mouse_y, const Camera& camera,
                const Terrain& terrain, const DefinitionRegistry& registry,
                const Entity* player);

    void Render(SDL_Renderer* renderer);

private:
    void RenderText(SDL_Renderer* renderer, const std::string& text, int x, int y);
    void ClearTextCache();

    TTF_Font* font_ = nullptr;

    // Text texture cache to avoid creating textures every frame
    struct CachedText {
        SDL_Texture* texture = nullptr;
        int w = 0, h = 0;
        std::string text;
    };
    std::unordered_map<int, CachedText> text_cache_;  // keyed by slot (y position)

    // Cached info
    std::string material_name_;
    std::string material_state_;
    int world_mouse_x_ = 0;
    int world_mouse_y_ = 0;

    // Player info
    std::string player_info_;
    std::string action_info_;

    // FPS
    int fps_ = 0;
    int frame_count_ = 0;
    uint32_t fps_last_time_ = 0;
};
