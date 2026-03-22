#pragma once
#include <SDL2/SDL.h>

enum class Action {
    MOVE_LEFT,
    MOVE_RIGHT,
    JUMP,
    DIG,      // primary dig action
};

class InputManager {
public:
    void poll(); // call once per frame, before update

    bool is_held   (Action a) const;
    bool is_pressed(Action a) const; // rising edge this frame only

    bool quit_requested() const { return m_quit; }

private:
    bool m_quit = false;
    // SDL keyboard state snapshot taken during poll()
    const Uint8* m_keys = nullptr;
    // Previous frame keyboard state for rising-edge detection
    bool m_prev[SDL_NUM_SCANCODES] = {};
};
