#include "input/InputManager.h"
#include <cstdio>

InputManager::InputManager(int num_slots)
    : raw_(std::make_unique<InputSystem>())
{
    if (num_slots < 1) num_slots = 1;
    maps_.resize(num_slots);  // Each PlayerInputMap constructor sets default bindings
}

void InputManager::PollEvents() {
    raw_->PollEvents();
}

float InputManager::GetAxis(int slot, InputAction action) const {
    if (!SlotValid(slot)) return 0.0f;
    return maps_[slot].GetAxis(action, *raw_);
}

bool InputManager::IsPressed(int slot, InputAction action) const {
    if (!SlotValid(slot)) return false;
    return maps_[slot].IsPressed(action, *raw_);
}

bool InputManager::IsJustPressed(int slot, InputAction action) const {
    if (!SlotValid(slot)) return false;
    return maps_[slot].IsJustPressed(action, *raw_);
}

bool InputManager::IsJustReleased(int slot, InputAction action) const {
    if (!SlotValid(slot)) return false;
    return maps_[slot].IsJustReleased(action, *raw_);
}

bool InputManager::ShouldQuit() const {
    return raw_->ShouldQuit();
}

int  InputManager::GetMouseX() const { return raw_->GetMouseX(); }
int  InputManager::GetMouseY() const { return raw_->GetMouseY(); }

bool InputManager::IsMouseDown(int button) const         { return raw_->IsMouseDown(button); }
bool InputManager::IsMouseJustPressed(int button) const  { return raw_->IsMouseJustPressed(button); }
bool InputManager::IsMouseJustReleased(int button) const { return raw_->IsMouseJustReleased(button); }

void InputManager::SetBinding(int slot, InputAction action, ActionBinding binding) {
    if (!SlotValid(slot)) {
        std::fprintf(stderr, "InputManager::SetBinding: invalid slot %d\n", slot);
        return;
    }
    maps_[slot].SetBinding(action, binding);
}
