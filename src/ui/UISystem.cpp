#include "ui/UISystem.h"

// ─── Lifecycle ────────────────────────────────────────────────────────────────

UISystem::UISystem(SDL_Renderer* renderer)
    : renderer_(std::make_unique<UIRenderer>(renderer))
{}

UISystem::~UISystem() = default;

// ─── Configuration (forwarded) ────────────────────────────────────────────────

void UISystem::LoadSkin(const std::string& path)                              { renderer_->LoadSkin(path); }
bool UISystem::LoadFont(const std::string& preferred_path, int size)          { return renderer_->LoadFont(preferred_path, size); }

// ─── Screen management ────────────────────────────────────────────────────────

std::shared_ptr<UIScreen> UISystem::CreateScreen(const std::string& name) {
    auto s = std::make_shared<UIScreen>();
    s->name = name;
    return s;
}

void UISystem::ShowScreen(std::shared_ptr<UIScreen> screen) {
    if (screen) screen_stack_.push_back(std::move(screen));
}

void UISystem::PopScreen() {
    if (screen_stack_.empty()) return;
    screen_stack_.pop_back();
    // A popped screen may have owned focused_input_ / pressed_element_; drop them.
    pressed_element_ = nullptr;
    if (focused_input_) {
        focused_input_ = nullptr;
        if (text_mode_cb_) text_mode_cb_(false);
    }
}

void UISystem::SetTextInputCallback(std::function<void(bool)> cb) {
    text_mode_cb_ = std::move(cb);
}

UIScreen* UISystem::TopScreen() {
    return screen_stack_.empty() ? nullptr : screen_stack_.back().get();
}

// ─── Tree walk ────────────────────────────────────────────────────────────────

void UISystem::ComputeAbs(UIElement& el, int parent_x, int parent_y) {
    el.abs_x = parent_x + el.x;
    el.abs_y = parent_y + el.y;
    for (auto& child : el.children) ComputeAbs(*child, el.abs_x, el.abs_y);
}

UIElement* UISystem::HitTest(UIElement& el, int mx, int my) {
    // Children first, back-to-front (last drawn is on top).
    for (auto it = el.children.rbegin(); it != el.children.rend(); ++it) {
        auto& child = *it;
        if (!child->visible) continue;
        if (UIElement* hit = HitTest(*child, mx, my)) return hit;
    }
    const bool interactive =
        (el.type == UIElementType::Button || el.type == UIElementType::Input);
    if (interactive && el.visible &&
        mx >= el.abs_x && mx < el.abs_x + el.w &&
        my >= el.abs_y && my < el.abs_y + el.h) {
        return &el;
    }
    return nullptr;
}

void UISystem::ClearHover(UIElement& el) {
    el.hovered = false;
    el.pressed = false;
    for (auto& child : el.children) ClearHover(*child);
}

void UISystem::RefreshTopScreenLayout() {
    if (auto* s = TopScreen()) ComputeAbs(*s->root, 0, 0);
}

// ─── Queries ──────────────────────────────────────────────────────────────────

bool UISystem::IsPointOverUI(int x, int y) {
    auto* s = TopScreen();
    if (!s) return false;
    ComputeAbs(*s->root, 0, 0);
    return HitTest(*s->root, x, y) != nullptr;
}

// ─── Input ────────────────────────────────────────────────────────────────────

void UISystem::HandleMouseMove(int x, int y) {
    auto* s = TopScreen();
    if (!s) return;

    ComputeAbs(*s->root, 0, 0);
    ClearHover(*s->root);

    if (UIElement* hovered = HitTest(*s->root, x, y)) {
        hovered->hovered = true;
        if (pressed_element_ == hovered) hovered->pressed = true;
    }
}

void UISystem::HandleMouseDown(int x, int y) {
    auto* s = TopScreen();
    if (!s) return;

    ComputeAbs(*s->root, 0, 0);
    UIElement* target = HitTest(*s->root, x, y);

    if (target && target->type == UIElementType::Input && !target->disabled) {
        if (focused_input_ && focused_input_ != target)
            focused_input_->focused = false;
        target->focused  = true;
        focused_input_   = target;
        pressed_element_ = target;
        if (text_mode_cb_) text_mode_cb_(true);
        return;
    }

    // Clicked outside any input → blur current focus.
    if (focused_input_) {
        focused_input_->focused = false;
        focused_input_ = nullptr;
        if (text_mode_cb_) text_mode_cb_(false);
    }
    if (target && !target->disabled) {
        target->pressed  = true;
        pressed_element_ = target;
    }
}

void UISystem::HandleMouseUp(int x, int y) {
    auto* s = TopScreen();
    if (!s) return;

    ComputeAbs(*s->root, 0, 0);
    UIElement* target = HitTest(*s->root, x, y);

    // Clear pressed_element_ BEFORE firing on_click — the callback may pop the
    // screen, invalidating both target and pressed_element_.
    UIElement* was_pressed = pressed_element_;
    pressed_element_ = nullptr;
    if (was_pressed) was_pressed->pressed = false;

    if (target && target == was_pressed && target->on_click) {
        auto fn = target->on_click;  // copy before call; may destroy self
        fn();
    }
}

void UISystem::HandleTextInput(const std::string& text) {
    if (!focused_input_ || text.empty()) return;
    auto& el = *focused_input_;
    for (char ch : text) {
        if (static_cast<int>(el.value.size()) >= el.max_length) break;
        el.value.insert(el.value.begin() + el.cursor, ch);
        el.cursor++;
    }
    if (el.on_change) el.on_change(el.value);
}

void UISystem::HandleKeyDown(SDL_Scancode key) {
    if (!focused_input_) return;
    auto& el = *focused_input_;
    switch (key) {
        case SDL_SCANCODE_BACKSPACE:
            if (el.cursor > 0) {
                el.value.erase(el.cursor - 1, 1);
                el.cursor--;
                if (el.on_change) el.on_change(el.value);
            }
            break;
        case SDL_SCANCODE_LEFT:
            if (el.cursor > 0) el.cursor--;
            break;
        case SDL_SCANCODE_RIGHT:
            if (el.cursor < static_cast<int>(el.value.size())) el.cursor++;
            break;
        case SDL_SCANCODE_ESCAPE:
            el.focused     = false;
            focused_input_ = nullptr;
            if (text_mode_cb_) text_mode_cb_(false);
            break;
        default: break;
    }
}

// ─── Render ───────────────────────────────────────────────────────────────────

void UISystem::Render() {
    auto* s = TopScreen();
    if (!s) return;
    ComputeAbs(*s->root, 0, 0);
    renderer_->RenderScreen(*s);
}
