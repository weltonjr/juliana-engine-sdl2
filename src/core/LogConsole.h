#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <unordered_map>

// Semi-transparent in-game log overlay. Toggle visibility with ` (backtick).
// Reads from EngineLog and renders the last N messages that fit on screen.
class LogConsole {
public:
    explicit LogConsole(SDL_Renderer* renderer);
    ~LogConsole();

    LogConsole(const LogConsole&) = delete;
    LogConsole& operator=(const LogConsole&) = delete;

    void Render(SDL_Renderer* renderer);

private:
    TTF_Font* font_ = nullptr;

    struct CachedText {
        SDL_Texture* texture = nullptr;
        int w = 0, h = 0;
    };
    // Keyed by message content — each unique string is rendered to a texture once.
    std::unordered_map<std::string, CachedText> text_cache_;

    void EnsureCached(SDL_Renderer* renderer, const std::string& text);
    void RenderLine(SDL_Renderer* renderer, const std::string& text, int x, int y);
};
