#pragma once

#include "input/InputSystem.h"
#include "input/InputAction.h"
#include "input/InputBinding.h"
#include "input/PlayerInputMap.h"
#include <vector>
#include <memory>

// Owns the raw InputSystem and one PlayerInputMap per player slot.
// Engine calls PollEvents() once per tick, then queries actions by slot index.
// Each slot has independent bindings, enabling splitscreen with different
// players on keyboard, gamepad, or other input sources.
class InputManager {
public:
    explicit InputManager(int num_slots = 1);

    void PollEvents();

    // Analog query — [-1, 1] for axis actions; 0 or 1 for button actions.
    float GetAxis(int slot, InputAction action) const;

    // Digital queries — IsPressed on an axis action returns true when |axis| > deadzone.
    bool IsPressed(int slot, InputAction action) const;
    bool IsJustPressed(int slot, InputAction action) const;
    bool IsJustReleased(int slot, InputAction action) const;

    bool ShouldQuit() const;
    int  GetMouseX() const;
    int  GetMouseY() const;

    // Mouse button queries (SDL_BUTTON_LEFT = 1 by default)
    bool IsMouseDown        (int button = SDL_BUTTON_LEFT) const;
    bool IsMouseJustPressed (int button = SDL_BUTTON_LEFT) const;
    bool IsMouseJustReleased(int button = SDL_BUTTON_LEFT) const;

    // Replace the full binding for an action at runtime (key rebinding / gamepad setup).
    void SetBinding(int slot, InputAction action, ActionBinding binding);

    // Access the raw layer (for direct scancode queries if needed).
    const InputSystem& GetRaw() const { return *raw_; }

private:
    std::unique_ptr<InputSystem> raw_;
    std::vector<PlayerInputMap>  maps_;

    bool SlotValid(int slot) const {
        return slot >= 0 && slot < static_cast<int>(maps_.size());
    }
};
