#include "ui/UIRenderer.h"
#include <cstdio>

// ─── Lifecycle ────────────────────────────────────────────────────────────────

UIRenderer::UIRenderer(SDL_Renderer* renderer)
    : renderer_(renderer)
{
    if (TTF_Init() < 0) {
        std::fprintf(stderr, "UIRenderer: TTF_Init failed: %s\n", TTF_GetError());
    }
}

UIRenderer::~UIRenderer() {
    if (font_) TTF_CloseFont(font_);
    TTF_Quit();
}

// ─── Configuration ────────────────────────────────────────────────────────────

void UIRenderer::LoadSkin(const std::string& path) {
    skin_ = UISkin::LoadFromFile(path);
}

bool UIRenderer::LoadFont(const std::string& preferred_path, int size) {
    if (!preferred_path.empty()) {
        font_ = TTF_OpenFont(preferred_path.c_str(), size);
        if (font_) return true;
        std::fprintf(stderr, "UIRenderer: could not load font '%s': %s\n",
                     preferred_path.c_str(), TTF_GetError());
    }

    const char* fallbacks[] = {
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Geneva.dfont",
        "/System/Library/Fonts/SFNSDisplay.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "C:/Windows/Fonts/arial.ttf",
        nullptr
    };
    for (int i = 0; fallbacks[i]; ++i) {
        font_ = TTF_OpenFont(fallbacks[i], size);
        if (font_) return true;
    }

    std::fprintf(stderr, "UIRenderer: no usable font found, text will not render\n");
    return false;
}

// ─── Rendering ────────────────────────────────────────────────────────────────

void UIRenderer::RenderScreen(UIScreen& screen) {
    if (screen.root && screen.root->visible) RenderElement(*screen.root);
}

void UIRenderer::RenderElement(const UIElement& el) {
    switch (el.type) {
        case UIElementType::Frame:  RenderFrame(el);  break;
        case UIElementType::Button: RenderButton(el); break;
        case UIElementType::Label:  RenderLabel(el);  break;
        case UIElementType::Input:  RenderInput(el);  break;
        case UIElementType::Image:  break;  // TODO: sprite rendering
    }
    for (auto& child : el.children) {
        if (child->visible) RenderElement(*child);
    }
}

void UIRenderer::RenderFrame(const UIElement& el) {
    // Transparent frames (w=0 or h=0) — used as pure containers — draw nothing.
    if (el.w <= 0 || el.h <= 0) return;
    DrawFilledRect(el.abs_x, el.abs_y, el.w, el.h, skin_.frame_bg);
    DrawRectBorder(el.abs_x, el.abs_y, el.w, el.h, skin_.frame_border);
}

void UIRenderer::RenderButton(const UIElement& el) {
    UIColor bg = el.disabled ? skin_.button_pressed
               : el.pressed  ? skin_.button_pressed
               : el.hovered  ? skin_.button_hover
               :               skin_.button_normal;
    DrawFilledRect(el.abs_x, el.abs_y, el.w, el.h, bg);
    DrawRectBorder(el.abs_x, el.abs_y, el.w, el.h, skin_.button_border);
    if (el.text.empty()) return;

    if (el.text_left) {
        const int PAD = 6;
        int ty = el.abs_y + (el.h - 14) / 2;
        DrawText(el.text, el.abs_x + PAD, ty, skin_.button_text.ToSDL());
    } else {
        DrawTextCentered(el.text, el.abs_x, el.abs_y, el.w, el.h, skin_.button_text.ToSDL());
    }
}

void UIRenderer::RenderLabel(const UIElement& el) {
    if (!el.text.empty())
        DrawText(el.text, el.abs_x, el.abs_y, skin_.label_text.ToSDL());
}

void UIRenderer::RenderInput(const UIElement& el) {
    UIColor bg     = el.disabled ? skin_.button_pressed : skin_.frame_bg;
    UIColor border = el.disabled ? skin_.frame_border
                   : el.focused  ? skin_.input_focus_border
                   :               skin_.frame_border;
    DrawFilledRect(el.abs_x, el.abs_y, el.w, el.h, bg);
    DrawRectBorder(el.abs_x, el.abs_y, el.w, el.h, border);

    const int PAD = 4;
    if (!el.value.empty()) {
        DrawText(el.value, el.abs_x + PAD, el.abs_y + (el.h - 14) / 2, skin_.button_text.ToSDL());
    } else if (!el.text.empty()) {
        DrawText(el.text, el.abs_x + PAD, el.abs_y + (el.h - 14) / 2, skin_.input_placeholder.ToSDL());
    }

    // Blinking caret while focused
    if (el.focused && (SDL_GetTicks() % 1000 < 500) && font_) {
        std::string before_cursor = el.value.substr(0, static_cast<size_t>(el.cursor));
        int tw = 0, th = 0;
        TTF_SizeUTF8(font_, before_cursor.empty() ? " " : before_cursor.c_str(), &tw, &th);
        int cx = el.abs_x + PAD + (before_cursor.empty() ? 0 : tw);
        int cy = el.abs_y + 3;
        SDL_SetRenderDrawColor(renderer_, border.r, border.g, border.b, 200);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        SDL_RenderDrawLine(renderer_, cx, cy, cx, cy + el.h - 6);
    }
}

// ─── Draw helpers ─────────────────────────────────────────────────────────────

void UIRenderer::DrawFilledRect(int x, int y, int w, int h, UIColor c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(renderer_, &rect);
}

void UIRenderer::DrawRectBorder(int x, int y, int w, int h, UIColor c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(renderer_, &rect);
}

void UIRenderer::DrawText(const std::string& text, int x, int y, SDL_Color color) {
    if (!font_) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font_, text.c_str(), color);
    if (!surf) {
        std::fprintf(stderr, "UIRenderer::DrawText: TTF_RenderUTF8_Blended failed: %s\n", TTF_GetError());
        return;
    }
    SDL_Surface* conv = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(surf);
    if (!conv) {
        std::fprintf(stderr, "UIRenderer::DrawText: SDL_ConvertSurfaceFormat failed: %s\n", SDL_GetError());
        return;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, conv);
    SDL_FreeSurface(conv);
    if (!tex) {
        std::fprintf(stderr, "UIRenderer::DrawText: SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
        return;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_Rect dst = {x, y, 0, 0};
    SDL_QueryTexture(tex, nullptr, nullptr, &dst.w, &dst.h);
    SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

void UIRenderer::DrawTextCentered(const std::string& text, int bx, int by,
                                  int bw, int bh, SDL_Color color) {
    if (!font_) return;
    int tw = 0, th = 0;
    TTF_SizeUTF8(font_, text.c_str(), &tw, &th);
    int x = bx + (bw - tw) / 2;
    int y = by + (bh - th) / 2;
    DrawText(text, x, y, color);
}
