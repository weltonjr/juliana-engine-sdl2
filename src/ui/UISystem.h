#pragma once
#include "ui/UIScreen.h"
#include "ui/UIRenderer.h"
#include <SDL2/SDL.h>
#include <functional>
#include <memory>
#include <vector>
#include <string>

// Manages the UI screen stack, routes mouse/keyboard input, and drives a
// UIRenderer to paint the top screen.
//
// Screen stack semantics:
//   ShowScreen(s)  — pushes s on top; only the top screen receives input/render
//   PopScreen()    — returns to the previous screen
//
// Call once per tick:  HandleMouseMove / HandleMouseDown / HandleMouseUp
// Call once per frame: Render
class UISystem {
public:
    explicit UISystem(SDL_Renderer* renderer);
    ~UISystem();

    // Skin/font are forwarded to the internal UIRenderer.
    void LoadSkin(const std::string& path);
    bool LoadFont(const std::string& preferred_path, int size);

    std::shared_ptr<UIScreen> CreateScreen(const std::string& name);
    void ShowScreen(std::shared_ptr<UIScreen> screen);
    void PopScreen();

    bool HasActiveScreen() const { return !screen_stack_.empty(); }

    // Returns true if (x, y) lies over an interactive element (Button/Input)
    // on the top screen. Lua uses this to suppress world-paint clicks when
    // the user is clicking UI.
    bool IsPointOverUI(int x, int y);

    // --- Input (call after InputManager::PollEvents) ---
    void HandleMouseMove (int x, int y);
    void HandleMouseDown (int x, int y);
    void HandleMouseUp   (int x, int y);
    void HandleTextInput (const std::string& text);
    void HandleKeyDown   (SDL_Scancode key);

    // Engine wires Start/StopTextInput when an Input element gains/loses focus.
    void SetTextInputCallback(std::function<void(bool)> cb);

    // Render the top screen (call once per frame, after the game world).
    void Render();

private:
    std::unique_ptr<UIRenderer>            renderer_;
    std::vector<std::shared_ptr<UIScreen>> screen_stack_;

    UIElement* pressed_element_ = nullptr;  // raw ptr; lifetime owned by screen_stack_
    UIElement* focused_input_   = nullptr;
    std::function<void(bool)> text_mode_cb_;

    // Tree-walk helpers — all operate on screen.root directly.
    void       ComputeAbs (UIElement& el, int parent_x, int parent_y);
    UIElement* HitTest    (UIElement& el, int mx, int my);
    void       ClearHover (UIElement& el);

    UIScreen* TopScreen();
    void      RefreshTopScreenLayout();
};
