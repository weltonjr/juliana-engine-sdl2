#pragma once

#include <SDL2/SDL.h>
#include <cstring>

class InputSystem {
public:
    InputSystem();

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

private:
    static constexpr int MAX_KEYS = 512;
    uint8_t  current_keys_[MAX_KEYS];
    uint8_t  previous_keys_[MAX_KEYS];
    uint32_t current_mouse_  = 0;
    uint32_t previous_mouse_ = 0;
    int  mouse_x_ = 0;
    int  mouse_y_ = 0;
    bool quit_ = false;
};
