#pragma once
#include "ui/UISkin.h"
#include "ui/UIScreen.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

// Renders UIScreens with SDL2. Owns the font, skin, and renderer handle.
// UISystem drives logic (input, hit-test, screen stack); UIRenderer just draws.
class UIRenderer {
public:
    explicit UIRenderer(SDL_Renderer* renderer);
    ~UIRenderer();

    UIRenderer(const UIRenderer&)            = delete;
    UIRenderer& operator=(const UIRenderer&) = delete;

    void LoadSkin(const std::string& path);

    // Returns true if any font was loaded (preferred path first, then fallbacks).
    bool LoadFont(const std::string& preferred_path, int size);

    // Caller must have already populated abs_x/abs_y on the element tree.
    void RenderScreen(UIScreen& screen);

private:
    void RenderElement(const UIElement& el);
    void RenderFrame  (const UIElement& el);
    void RenderButton (const UIElement& el);
    void RenderLabel  (const UIElement& el);
    void RenderInput  (const UIElement& el);

    void DrawFilledRect(int x, int y, int w, int h, UIColor c);
    void DrawRectBorder(int x, int y, int w, int h, UIColor c);
    void DrawText(const std::string& text, int x, int y, SDL_Color color);
    void DrawTextCentered(const std::string& text, int bx, int by,
                          int bw, int bh, SDL_Color color);

    SDL_Renderer* renderer_ = nullptr;
    TTF_Font*     font_     = nullptr;
    UISkin        skin_;
};
