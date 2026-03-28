#include "input/PlayerInputMap.h"
#include <cmath>

PlayerInputMap::PlayerInputMap() {
    using A  = InputAction;
    using AB = ActionBinding;

    // --- Axis actions ---
    {
        AB& b     = bindings_[static_cast<int>(A::MoveX)];
        b.type    = AB::Type::Axis;
        b.axis    = { SDL_SCANCODE_A, SDL_SCANCODE_D, 0.15f };
    }

    // --- Button actions (type defaults to Button) ---
    bindings_[static_cast<int>(A::Jump)].button         = { SDL_SCANCODE_W,
                                                              SDL_SCANCODE_UNKNOWN,
                                                              SDL_SCANCODE_SPACE };
    bindings_[static_cast<int>(A::DigDown)].button      = { SDL_SCANCODE_Q,
                                                              SDL_SCANCODE_S };   // Q+S combo
    bindings_[static_cast<int>(A::DigHorizontal)].button = { SDL_SCANCODE_C };
    bindings_[static_cast<int>(A::PrevCharacter)].button = { SDL_SCANCODE_1 };
    bindings_[static_cast<int>(A::NextCharacter)].button = { SDL_SCANCODE_3 };
    bindings_[static_cast<int>(A::Quit)].button          = { SDL_SCANCODE_ESCAPE };
}

void PlayerInputMap::SetBinding(InputAction action, ActionBinding binding) {
    bindings_[static_cast<int>(action)] = binding;
}

const ActionBinding& PlayerInputMap::GetBinding(InputAction action) const {
    return bindings_[static_cast<int>(action)];
}

float PlayerInputMap::GetAxis(InputAction action, const InputSystem& raw) const {
    const ActionBinding& b = GetBinding(action);

    if (b.type == ActionBinding::Type::Axis) {
        float value = 0.0f;
        if (b.axis.negative_key != SDL_SCANCODE_UNKNOWN && raw.IsKeyDown(b.axis.negative_key))
            value -= 1.0f;
        if (b.axis.positive_key != SDL_SCANCODE_UNKNOWN && raw.IsKeyDown(b.axis.positive_key))
            value += 1.0f;
        // Apply deadzone (meaningful for analog gamepad values; keyboard always hits ±1)
        if (std::abs(value) <= b.axis.deadzone)
            value = 0.0f;
        return value;
    }

    // Button action: return 0 or 1
    return IsPressed(action, raw) ? 1.0f : 0.0f;
}

bool PlayerInputMap::IsPressed(InputAction action, const InputSystem& raw) const {
    const ActionBinding& b = GetBinding(action);

    if (b.type == ActionBinding::Type::Axis) {
        return std::abs(GetAxis(action, raw)) > b.axis.deadzone;
    }

    const ButtonBinding& btn = b.button;
    if (btn.primary == SDL_SCANCODE_UNKNOWN) return false;

    bool primary_held = raw.IsKeyDown(btn.primary);

    if (btn.modifier != SDL_SCANCODE_UNKNOWN)
        return primary_held && raw.IsKeyDown(btn.modifier);
    if (btn.alt != SDL_SCANCODE_UNKNOWN)
        return primary_held || raw.IsKeyDown(btn.alt);
    return primary_held;
}

bool PlayerInputMap::IsJustPressed(InputAction action, const InputSystem& raw) const {
    const ActionBinding& b = GetBinding(action);

    if (b.type == ActionBinding::Type::Axis) {
        // Fires when either contributing key is just pressed
        bool just = false;
        if (b.axis.negative_key != SDL_SCANCODE_UNKNOWN)
            just |= raw.IsJustPressed(b.axis.negative_key);
        if (b.axis.positive_key != SDL_SCANCODE_UNKNOWN)
            just |= raw.IsJustPressed(b.axis.positive_key);
        return just;
    }

    const ButtonBinding& btn = b.button;
    if (btn.primary == SDL_SCANCODE_UNKNOWN) return false;

    bool primary_just = raw.IsJustPressed(btn.primary);

    if (btn.modifier != SDL_SCANCODE_UNKNOWN) {
        // Combo fires when either key completes the pair
        return (primary_just && raw.IsKeyDown(btn.modifier)) ||
               (raw.IsJustPressed(btn.modifier) && raw.IsKeyDown(btn.primary));
    }
    if (btn.alt != SDL_SCANCODE_UNKNOWN)
        return primary_just || raw.IsJustPressed(btn.alt);
    return primary_just;
}

bool PlayerInputMap::IsJustReleased(InputAction action, const InputSystem& raw) const {
    const ActionBinding& b = GetBinding(action);

    if (b.type == ActionBinding::Type::Axis) {
        bool released = false;
        if (b.axis.negative_key != SDL_SCANCODE_UNKNOWN)
            released |= raw.IsJustReleased(b.axis.negative_key);
        if (b.axis.positive_key != SDL_SCANCODE_UNKNOWN)
            released |= raw.IsJustReleased(b.axis.positive_key);
        return released;
    }

    const ButtonBinding& btn = b.button;
    if (btn.primary == SDL_SCANCODE_UNKNOWN) return false;

    bool primary_released = raw.IsJustReleased(btn.primary);

    if (btn.modifier != SDL_SCANCODE_UNKNOWN)
        return primary_released || raw.IsJustReleased(btn.modifier);
    if (btn.alt != SDL_SCANCODE_UNKNOWN)
        return primary_released || raw.IsJustReleased(btn.alt);
    return primary_released;
}
