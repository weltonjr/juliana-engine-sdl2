#include "core/LogConsole.h"
#include "core/EngineLog.h"
#include <algorithm>
#include <cstdio>

static const char* kFontPaths[] = {
    "/System/Library/Fonts/SFNSMono.ttf",
    "/System/Library/Fonts/Menlo.ttc",
    "/System/Library/Fonts/Monaco.ttf",
    "/System/Library/Fonts/Courier.ttc",
    "/System/Library/Fonts/Supplemental/Courier New.ttf",
    "C:/Windows/Fonts/consola.ttf",
    nullptr
};

LogConsole::LogConsole(SDL_Renderer* /*renderer*/) {
    // TTF_Init is ref-counted; safe to call multiple times.
    if (TTF_Init() < 0) {
        std::fprintf(stderr, "LogConsole: TTF_Init failed: %s\n", TTF_GetError());
        return;
    }
    for (int i = 0; kFontPaths[i]; i++) {
        font_ = TTF_OpenFont(kFontPaths[i], 13);
        if (font_) break;
    }
    if (!font_)
        std::fprintf(stderr, "LogConsole: could not load any system font\n");
}

LogConsole::~LogConsole() {
    for (auto& [text, cached] : text_cache_)
        if (cached.texture) SDL_DestroyTexture(cached.texture);
    if (font_) TTF_CloseFont(font_);
    TTF_Quit();
}

void LogConsole::EnsureCached(SDL_Renderer* renderer, const std::string& text) {
    if (text_cache_.count(text) || !font_ || text.empty()) return;

    SDL_Color color = {180, 255, 180, 255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font_, text.c_str(), color);
    if (!surf) return;

    CachedText cached;
    cached.texture = SDL_CreateTextureFromSurface(renderer, surf);
    cached.w = surf->w;
    cached.h = surf->h;
    SDL_FreeSurface(surf);
    text_cache_[text] = cached;
}

void LogConsole::RenderLine(SDL_Renderer* renderer, const std::string& text, int x, int y) {
    EnsureCached(renderer, text);
    auto it = text_cache_.find(text);
    if (it == text_cache_.end() || !it->second.texture) return;
    SDL_Rect dst = {x, y, it->second.w, it->second.h};
    SDL_RenderCopy(renderer, it->second.texture, nullptr, &dst);
}

void LogConsole::Render(SDL_Renderer* renderer) {
    int win_w = 0, win_h = 0;
    SDL_RenderGetLogicalSize(renderer, &win_w, &win_h);
    // Some backends return 0 (or tiny values) here; fallback so panel always shows.
    if (win_w < 100 || win_h < 100) {
        SDL_GetRendererOutputSize(renderer, &win_w, &win_h);
    }

    const int margin   = 10;
    const int padding  = 8;
    const int line_h   = 16;
    const int header_h = 22;

    int px = margin;
    int pw = std::max(1, win_w - margin * 2);
    int ph = win_h / 3;
    ph = std::max(ph, header_h + padding * 2 + line_h);
    ph = std::min(ph, std::max(1, win_h - margin * 2));
    int py = std::max(margin, win_h - margin - ph);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Background
    SDL_SetRenderDrawColor(renderer, 8, 8, 18, 210);
    SDL_Rect panel = {px, py, pw, ph};
    SDL_RenderFillRect(renderer, &panel);

    // Header bar
    SDL_SetRenderDrawColor(renderer, 25, 25, 55, 230);
    SDL_Rect header = {px, py, pw, header_h};
    SDL_RenderFillRect(renderer, &header);

    // Border
    SDL_SetRenderDrawColor(renderer, 70, 70, 160, 220);
    SDL_RenderDrawRect(renderer, &panel);

    // Header label — rendered fresh each frame (one call, negligible cost)
    if (font_) {
        SDL_Color hc = {160, 160, 240, 255};
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font_, "ENGINE LOG  (` to close)", hc);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            int tx = px + padding;
            int ty = py + (header_h - surf->h) / 2;
            SDL_Rect dst = {tx, ty, surf->w, surf->h};
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
    }

    // Message area
    int content_x = px + padding;
    int content_y = py + header_h + padding;
    int content_h = ph - header_h - padding * 2;
    int max_lines = content_h / line_h;

    const auto& msgs = EngineLog::GetMessages();
    int total = static_cast<int>(msgs.size());
    int start = std::max(0, total - max_lines);

    for (int i = start; i < total; i++) {
        int y = content_y + (i - start) * line_h;
        RenderLine(renderer, msgs[i], content_x, y);
    }
}
