#pragma once

// Game-concept input actions. Physical bindings live in PlayerInputMap.
// Engine and gameplay code query these actions — never raw SDL scancodes.
//
// Two kinds:
//   Axis actions   — use GetAxis() for a float in [-1, 1]; keyboard maps to -1/0/+1.
//   Button actions — use IsPressed() / IsJustPressed() / IsJustReleased().
//
// IsPressed() on an axis action returns true when |GetAxis()| > deadzone.
enum class InputAction {
    // --- Axis actions ---
    MoveX,          // -1 = left, +1 = right  |  keyboard: A / D  |  gamepad: left stick X

    // --- Button actions ---
    Jump,           // default: W or Space
    DigDown,        // default: Q + S (combo)
    DigHorizontal,  // default: C  (game code checks MoveX direction)
    PrevCharacter,  // default: 1
    NextCharacter,  // default: 3
    Quit,           // default: Escape

    Count  // sentinel — keep last
};
