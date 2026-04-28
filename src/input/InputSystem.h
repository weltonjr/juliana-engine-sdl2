#pragma once

#include <SDL2/SDL.h>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

class InputSystem {
public:
    InputSystem();

    // Listener fires for every polled SDL event before InputSystem aggregates it.
    // Used by the RmlUi backend so the UI sees raw SDL input on the same path
    // that real users / tests drive the engine through.
    using EventListener = std::function<void(SDL_Event&)>;
    void AddEventListener(EventListener cb) { event_listeners_.push_back(std::move(cb)); }

    void PollEvents();

    bool IsKeyDown(SDL_Scancode key) const;
    bool IsJustPressed(SDL_Scancode key) const;
    bool IsJustReleased(SDL_Scancode key) const;
    bool ShouldQuit() const { return quit_; }

    int  GetMouseX() const { return mouse_x_; }
    int  GetMouseY() const { return mouse_y_; }

    // Mouse button queries — use SDL_BUTTON_LEFT (1), SDL_BUTTON_RIGHT (3), etc.
    bool IsMouseDown       (int button = SDL_BUTTON_LEFT) const;
    bool IsMouseJustPressed(int button = SDL_BUTTON_LEFT) const;
    bool IsMouseJustReleased(int button = SDL_BUTTON_LEFT) const;

    // Scroll wheel — accumulated ticks this frame (positive = scroll up/away)
    int GetScrollY() const { return scroll_y_; }

    // Text input — UTF-8 characters typed this frame (populated when text input mode is active)
    const std::string& GetTextInput() const { return text_input_; }
    void StartTextInput();
    void StopTextInput();
    bool IsTextInputActive() const { return text_input_active_; }

private:
    static constexpr int MAX_KEYS = 512;
    uint8_t  current_keys_[MAX_KEYS];
    uint8_t  previous_keys_[MAX_KEYS];
    uint32_t current_mouse_  = 0;
    uint32_t previous_mouse_ = 0;
    int  mouse_x_ = 0;
    int  mouse_y_ = 0;
    bool quit_ = false;

    int         scroll_y_ = 0;
    std::string text_input_;
    bool        text_input_active_ = false;

    std::vector<EventListener> event_listeners_;
};
