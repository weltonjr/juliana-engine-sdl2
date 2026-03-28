#pragma once

#include "input/InputAction.h"
#include "input/InputBinding.h"
#include "input/InputSystem.h"
#include <array>

// Stores one ActionBinding per InputAction for a single player slot.
// Default bindings are set for a keyboard layout.
// Bindings can be replaced at runtime for key rebinding or gamepad support.
class PlayerInputMap {
public:
    PlayerInputMap();

    void SetBinding(InputAction action, ActionBinding binding);
    const ActionBinding& GetBinding(InputAction action) const;

    // Analog query — returns [-1, 1] for axis actions, 0 or 1 for button actions.
    float GetAxis(InputAction action, const InputSystem& raw) const;

    // Digital queries — for axis actions, IsPressed() returns true when |axis| > deadzone.
    bool IsPressed(InputAction action, const InputSystem& raw) const;
    bool IsJustPressed(InputAction action, const InputSystem& raw) const;
    bool IsJustReleased(InputAction action, const InputSystem& raw) const;

private:
    static constexpr int ACTION_COUNT = static_cast<int>(InputAction::Count);
    std::array<ActionBinding, ACTION_COUNT> bindings_;
};
