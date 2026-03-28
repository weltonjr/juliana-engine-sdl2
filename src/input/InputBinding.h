#pragma once

#include <SDL2/SDL.h>

// -----------------------------------------------------------------------
// ButtonBinding — maps a button/key action to physical keys.
//   primary:   main key that triggers the action.
//   modifier:  if set, both primary AND modifier must be held (combo, e.g. Q+S).
//   alt:       secondary key that alone also triggers the action (e.g. Space for Jump).
//
// Future gamepad fields to add here:
//   SDL_GameControllerButton gamepad_button = SDL_CONTROLLER_BUTTON_INVALID;
// -----------------------------------------------------------------------
struct ButtonBinding {
    SDL_Scancode primary  = SDL_SCANCODE_UNKNOWN;
    SDL_Scancode modifier = SDL_SCANCODE_UNKNOWN;
    SDL_Scancode alt      = SDL_SCANCODE_UNKNOWN;
};

// -----------------------------------------------------------------------
// AxisBinding — maps an axis action to a signed float in [-1, 1].
//   negative_key:  key that contributes -1 (e.g. A for MoveX).
//   positive_key:  key that contributes +1 (e.g. D for MoveX).
//   deadzone:      values with |axis| <= deadzone are clamped to 0.
//                  Relevant for gamepad analog sticks; ignored for keyboard.
//
// Future gamepad fields to add here:
//   SDL_GameControllerAxis gamepad_axis = SDL_CONTROLLER_AXIS_INVALID;
//   bool invert = false;
// -----------------------------------------------------------------------
struct AxisBinding {
    SDL_Scancode negative_key = SDL_SCANCODE_UNKNOWN;
    SDL_Scancode positive_key = SDL_SCANCODE_UNKNOWN;
    float        deadzone     = 0.15f;
};

// -----------------------------------------------------------------------
// ActionBinding — unified binding for one InputAction.
// The type field selects which binding is active.
// -----------------------------------------------------------------------
struct ActionBinding {
    enum class Type { Button, Axis };
    Type          type   = Type::Button;
    ButtonBinding button;
    AxisBinding   axis;
};
