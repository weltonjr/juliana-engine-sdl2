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

    int GetMouseX() const { return mouse_x_; }
    int GetMouseY() const { return mouse_y_; }

private:
    static constexpr int MAX_KEYS = 512;
    uint8_t current_keys_[MAX_KEYS];
    uint8_t previous_keys_[MAX_KEYS];
    int mouse_x_ = 0;
    int mouse_y_ = 0;
    bool quit_ = false;
};
