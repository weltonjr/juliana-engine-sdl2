#pragma once
#include "ui/UISkin.h"
#include "ui/UIScreen.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <functional>
#include <memory>
#include <vector>
#include <string>

// Manages the UI screen stack, processes mouse input, and renders all active screens.
//
// Screen stack semantics:
//   ShowScreen(s)  — pushes s on top; only the top screen receives input & is rendered
//   PopScreen()    — returns to the previous screen
//
// Call once per tick: HandleMouseMove / HandleMouseDown / HandleMouseUp
// Call once per frame: Render
class UISystem {
public:
    explicit UISystem(SDL_Renderer* renderer);
    ~UISystem();

    // Load a skin from a .toml file; safe to call before/after font loading
    void LoadSkin(const std::string& path);

    // Try the given TTF path first, then fall back to common system fonts.
    // Returns true if any font was loaded.
    bool LoadFont(const std::string& preferred_path, int size);

    std::shared_ptr<UIScreen> CreateScreen(const std::string& name);
    void ShowScreen(std::shared_ptr<UIScreen> screen);
    void PopScreen();

    bool HasActiveScreen() const { return !screen_stack_.empty(); }

    // Returns true if (x, y) lies over an interactive UI element (Button/Input)
    // on the top screen. Useful for preventing game-world clicks from leaking
    // through the UI (e.g. painting the map while clicking a menu button).
    bool IsPointOverUI(int x, int y);

    // --- Input (call after InputManager::PollEvents) ---
    void HandleMouseMove (int x, int y);
    void HandleMouseDown (int x, int y);
    void HandleMouseUp   (int x, int y);
    void HandleTextInput (const std::string& text);
    void HandleKeyDown   (SDL_Scancode key);

    // Called by Engine to wire Start/StopTextInput when an Input element gains/loses focus
    void SetTextInputCallback(std::function<void(bool)> cb);

    // --- Render (call once per frame, after game world rendering) ---
    void Render();

private:
    SDL_Renderer* renderer_;
    TTF_Font*     font_ = nullptr;
    UISkin        skin_;

    std::vector<std::shared_ptr<UIScreen>> screen_stack_;

    int  mouse_x_ = 0, mouse_y_ = 0;
    bool mouse_down_ = false;
    UIElement* pressed_element_ = nullptr;  // raw ptr; lifetime owned by screen_stack_
    UIElement* focused_input_   = nullptr;  // currently focused Input element
    std::function<void(bool)> text_mode_cb_;  // called with true/false on focus gain/loss

    // --- Absolute position computation ---
    void ComputeAbsPositions(UIElement& el, int parent_x, int parent_y);
    void ComputeScreenAbsPositions(UIScreen& screen);

    // --- Hit testing ---
    UIElement* FindButtonAt(UIElement& el, int mx, int my);
    UIElement* FindButtonAtScreen(UIScreen& screen, int mx, int my);
    void       ClearButtonStates(UIElement& el);
    void       ClearScreenButtonStates(UIScreen& screen);

    // --- Rendering ---
    void RenderScreen (UIScreen& screen);
    void RenderElement(const UIElement& el);
    void RenderFrame  (const UIElement& el);
    void RenderButton (const UIElement& el);
    void RenderLabel  (const UIElement& el);
    void RenderInput  (const UIElement& el);

    void DrawFilledRect(int x, int y, int w, int h, UIColor color);
    void DrawRectBorder(int x, int y, int w, int h, UIColor color);
    void DrawText(const std::string& text, int x, int y, SDL_Color color);
    void DrawTextCentered(const std::string& text, int cx, int cy, int w, int h, SDL_Color color);
};
