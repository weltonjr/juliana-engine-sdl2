#pragma once

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

private:
    bool m_held[4]    = {};
    bool m_pressed[4] = {};
};
