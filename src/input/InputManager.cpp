#include "InputManager.h"
#include "raylib.h"

// Maps each Action to one or two raylib keys (either is enough)
static int key_a(Action a) {
    switch (a) {
        case Action::MOVE_LEFT:  return KEY_A;
        case Action::MOVE_RIGHT: return KEY_D;
        case Action::JUMP:       return KEY_SPACE;
        case Action::DIG:        return KEY_C;
    }
    return 0;
}
static int key_b(Action a) {
    switch (a) {
        case Action::MOVE_LEFT:  return KEY_LEFT;
        case Action::MOVE_RIGHT: return KEY_RIGHT;
        case Action::JUMP:       return KEY_UP;
        case Action::DIG:        return KEY_LEFT_CONTROL;
    }
    return 0;
}

void InputManager::poll() {
    constexpr int N = 4;
    for (int i = 0; i < N; i++) {
        Action a = static_cast<Action>(i);
        bool held = IsKeyDown(key_a(a)) || IsKeyDown(key_b(a));
        m_pressed[i] = held && !m_held[i]; // true only on the first frame
        m_held[i]    = held;
    }
}

bool InputManager::is_held   (Action a) const { return m_held   [static_cast<int>(a)]; }
bool InputManager::is_pressed(Action a) const { return m_pressed[static_cast<int>(a)]; }
