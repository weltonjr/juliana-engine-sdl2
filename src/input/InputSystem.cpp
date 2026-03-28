#include "input/InputSystem.h"

InputSystem::InputSystem() {
    std::memset(current_keys_, 0, sizeof(current_keys_));
    std::memset(previous_keys_, 0, sizeof(previous_keys_));
}

void InputSystem::PollEvents() {
    // Copy current to previous
    std::memcpy(previous_keys_, current_keys_, sizeof(current_keys_));

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            quit_ = true;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
            quit_ = true;
        }
    }

    // Mouse position
    SDL_GetMouseState(&mouse_x_, &mouse_y_);

    // Snapshot current keyboard state
    int num_keys = 0;
    const uint8_t* state = SDL_GetKeyboardState(&num_keys);
    int copy_count = num_keys < MAX_KEYS ? num_keys : MAX_KEYS;
    std::memcpy(current_keys_, state, copy_count);
}

bool InputSystem::IsKeyDown(SDL_Scancode key) const {
    if (key >= MAX_KEYS) return false;
    return current_keys_[key] != 0;
}

bool InputSystem::IsJustPressed(SDL_Scancode key) const {
    if (key >= MAX_KEYS) return false;
    return current_keys_[key] && !previous_keys_[key];
}

bool InputSystem::IsJustReleased(SDL_Scancode key) const {
    if (key >= MAX_KEYS) return false;
    return !current_keys_[key] && previous_keys_[key];
}
