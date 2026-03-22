#include "InputManager.h"
#include <cstring>

// Maps Action to primary SDL scancode
static SDL_Scancode scancode_a(Action a) {
    switch (a) {
        case Action::MOVE_LEFT:  return SDL_SCANCODE_A;
        case Action::MOVE_RIGHT: return SDL_SCANCODE_D;
        case Action::JUMP:       return SDL_SCANCODE_SPACE;
        case Action::DIG:        return SDL_SCANCODE_C;
    }
    return SDL_SCANCODE_UNKNOWN;
}

// Maps Action to alternate SDL scancode
static SDL_Scancode scancode_b(Action a) {
    switch (a) {
        case Action::MOVE_LEFT:  return SDL_SCANCODE_LEFT;
        case Action::MOVE_RIGHT: return SDL_SCANCODE_RIGHT;
        case Action::JUMP:       return SDL_SCANCODE_UP;
        case Action::DIG:        return SDL_SCANCODE_LCTRL;
    }
    return SDL_SCANCODE_UNKNOWN;
}

void InputManager::poll() {
    // Save previous keyboard state for rising-edge detection
    if (m_keys) {
        std::memcpy(m_prev, m_keys, SDL_NUM_SCANCODES * sizeof(bool));
    }

    // Pump SDL events (needed to update keyboard state)
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) m_quit = true;
        if (e.type == SDL_KEYDOWN &&
            e.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
            m_quit = true;
    }

    m_keys = SDL_GetKeyboardState(nullptr);
}

bool InputManager::is_held(Action a) const {
    if (!m_keys) return false;
    SDL_Scancode sa = scancode_a(a);
    SDL_Scancode sb = scancode_b(a);
    return (sa != SDL_SCANCODE_UNKNOWN && m_keys[sa]) ||
           (sb != SDL_SCANCODE_UNKNOWN && m_keys[sb]);
}

bool InputManager::is_pressed(Action a) const {
    if (!m_keys) return false;
    SDL_Scancode sa = scancode_a(a);
    SDL_Scancode sb = scancode_b(a);
    bool held_now = (sa != SDL_SCANCODE_UNKNOWN && m_keys[sa]) ||
                    (sb != SDL_SCANCODE_UNKNOWN && m_keys[sb]);
    bool held_prev = (sa != SDL_SCANCODE_UNKNOWN && m_prev[sa]) ||
                     (sb != SDL_SCANCODE_UNKNOWN && m_prev[sb]);
    return held_now && !held_prev;
}
