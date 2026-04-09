#include "ui/UISystem.h"
#include <algorithm>
#include <cstdio>

// ─── Lifecycle ───────────────────────────────────────────────────────────────

UISystem::UISystem(SDL_Renderer* renderer)
    : renderer_(renderer)
{
    if (TTF_Init() < 0) {
        std::fprintf(stderr, "UISystem: TTF_Init failed: %s\n", TTF_GetError());
    }
}

UISystem::~UISystem() {
    if (font_) TTF_CloseFont(font_);
    TTF_Quit();
}

// ─── Configuration ────────────────────────────────────────────────────────────

void UISystem::LoadSkin(const std::string& path) {
    skin_ = UISkin::LoadFromFile(path);
}

bool UISystem::LoadFont(const std::string& preferred_path, int size) {
    // Try the game-provided font first
    if (!preferred_path.empty()) {
        font_ = TTF_OpenFont(preferred_path.c_str(), size);
        if (font_) return true;
        std::fprintf(stderr, "UISystem: could not load font '%s': %s\n",
                     preferred_path.c_str(), TTF_GetError());
    }

    // Fall back to common macOS system fonts
    const char* fallbacks[] = {
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Geneva.dfont",
        "/System/Library/Fonts/SFNSDisplay.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",  // Linux
        "C:/Windows/Fonts/arial.ttf",                       // Windows
        nullptr
    };
    for (int i = 0; fallbacks[i]; ++i) {
        font_ = TTF_OpenFont(fallbacks[i], size);
        if (font_) return true;
    }

    std::fprintf(stderr, "UISystem: no usable font found, text will not render\n");
    return false;
}

// ─── Screen management ────────────────────────────────────────────────────────

std::shared_ptr<UIScreen> UISystem::CreateScreen(const std::string& name) {
    return std::make_shared<UIScreen>(UIScreen{name, {}});
}

void UISystem::ShowScreen(std::shared_ptr<UIScreen> screen) {
    if (screen) screen_stack_.push_back(std::move(screen));
}

void UISystem::PopScreen() {
    if (!screen_stack_.empty()) screen_stack_.pop_back();
}

void UISystem::SetTextInputCallback(std::function<void(bool)> cb) {
    text_mode_cb_ = std::move(cb);
}

// ─── Absolute position computation ───────────────────────────────────────────

void UISystem::ComputeAbsPositions(UIElement& el, int parent_x, int parent_y) {
    el.abs_x = parent_x + el.x;
    el.abs_y = parent_y + el.y;
    for (auto& child : el.children)
        ComputeAbsPositions(*child, el.abs_x, el.abs_y);
}

void UISystem::ComputeScreenAbsPositions(UIScreen& screen) {
    for (auto& el : screen.root_elements)
        ComputeAbsPositions(*el, 0, 0);
}

// ─── Hit testing ─────────────────────────────────────────────────────────────

UIElement* UISystem::FindButtonAt(UIElement& el, int mx, int my) {
    // Check children depth-first, back-to-front (last child rendered on top)
    for (auto it = el.children.rbegin(); it != el.children.rend(); ++it) {
        auto& child = *it;
        if (!child->visible) continue;
        UIElement* found = FindButtonAt(*child, mx, my);
        if (found) return found;
    }
    bool interactive = (el.type == UIElementType::Button || el.type == UIElementType::Input);
    if (interactive && el.visible) {
        if (mx >= el.abs_x && mx < el.abs_x + el.w &&
            my >= el.abs_y && my < el.abs_y + el.h)
            return &el;
    }
    return nullptr;
}

UIElement* UISystem::FindButtonAtScreen(UIScreen& screen, int mx, int my) {
    for (auto it = screen.root_elements.rbegin(); it != screen.root_elements.rend(); ++it) {
        UIElement* found = FindButtonAt(**it, mx, my);
        if (found) return found;
    }
    return nullptr;
}

void UISystem::ClearButtonStates(UIElement& el) {
    el.hovered = false;
    el.pressed  = false;
    for (auto& child : el.children) ClearButtonStates(*child);
}

void UISystem::ClearScreenButtonStates(UIScreen& screen) {
    for (auto& el : screen.root_elements) ClearButtonStates(*el);
}

// ─── Input ────────────────────────────────────────────────────────────────────

void UISystem::HandleMouseMove(int x, int y) {
    mouse_x_ = x;
    mouse_y_ = y;
    if (screen_stack_.empty()) return;

    UIScreen& screen = *screen_stack_.back();
    ComputeScreenAbsPositions(screen);   // ensure abs positions are fresh
    ClearScreenButtonStates(screen);

    UIElement* hovered = FindButtonAtScreen(screen, x, y);
    if (hovered) {
        hovered->hovered = true;
        // Keep pressed state if this was the element being held
        if (pressed_element_ == hovered)
            hovered->pressed = true;
    }
}

void UISystem::HandleMouseDown(int x, int y) {
    mouse_down_ = true;
    if (screen_stack_.empty()) return;

    UIScreen& screen = *screen_stack_.back();
    ComputeScreenAbsPositions(screen);
    UIElement* target = FindButtonAtScreen(screen, x, y);

    if (target && target->type == UIElementType::Input) {
        // Blur previously focused input
        if (focused_input_ && focused_input_ != target) {
            focused_input_->focused = false;
        }
        // Focus this input
        target->focused  = true;
        focused_input_   = target;
        pressed_element_ = target;
        if (text_mode_cb_) text_mode_cb_(true);
    } else {
        // Clicked outside an input — blur current focus
        if (focused_input_) {
            focused_input_->focused = false;
            focused_input_ = nullptr;
            if (text_mode_cb_) text_mode_cb_(false);
        }
        if (target) {
            target->pressed  = true;
            pressed_element_ = target;
        }
    }
}

void UISystem::HandleTextInput(const std::string& text) {
    if (!focused_input_ || text.empty()) return;
    // Insert characters at cursor, respecting max_length
    for (char ch : text) {
        if (static_cast<int>(focused_input_->value.size()) >= focused_input_->max_length) break;
        focused_input_->value.insert(focused_input_->value.begin() + focused_input_->cursor, ch);
        focused_input_->cursor++;
    }
    if (focused_input_->on_change) focused_input_->on_change(focused_input_->value);
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
            el.focused    = false;
            focused_input_ = nullptr;
            if (text_mode_cb_) text_mode_cb_(false);
            break;
        default: break;
    }
}

void UISystem::HandleMouseUp(int x, int y) {
    mouse_down_ = false;
    if (screen_stack_.empty()) return;

    UIScreen& screen = *screen_stack_.back();
    ComputeScreenAbsPositions(screen);
    UIElement* target = FindButtonAtScreen(screen, x, y);

    // Fire on_click only if released over the same element that was pressed
    if (target && target == pressed_element_ && target->on_click) {
        target->on_click();
    }
    if (pressed_element_) {
        pressed_element_->pressed = false;
        pressed_element_ = nullptr;
    }
}

// ─── Rendering ────────────────────────────────────────────────────────────────

void UISystem::Render() {
    if (screen_stack_.empty()) return;
    UIScreen& screen = *screen_stack_.back();
    ComputeScreenAbsPositions(screen);
    RenderScreen(screen);
}

void UISystem::RenderScreen(UIScreen& screen) {
    for (auto& el : screen.root_elements) {
        if (el->visible) RenderElement(*el);
    }
}

void UISystem::RenderElement(const UIElement& el) {
    switch (el.type) {
        case UIElementType::Frame:  RenderFrame(el);  break;
        case UIElementType::Button: RenderButton(el); break;
        case UIElementType::Label:  RenderLabel(el);  break;
        case UIElementType::Input:  RenderInput(el);  break;
        case UIElementType::Image:  break;  // TODO: sprite rendering
    }
    for (auto& child : el.children) {
        if (child->visible) RenderElement(*child);
    }
}

void UISystem::RenderFrame(const UIElement& el) {
    DrawFilledRect(el.abs_x, el.abs_y, el.w, el.h, skin_.frame_bg);
    DrawRectBorder(el.abs_x, el.abs_y, el.w, el.h, skin_.frame_border);
}

void UISystem::RenderButton(const UIElement& el) {
    UIColor bg = el.pressed ? skin_.button_pressed
               : el.hovered ? skin_.button_hover
               : skin_.button_normal;
    DrawFilledRect(el.abs_x, el.abs_y, el.w, el.h, bg);
    DrawRectBorder(el.abs_x, el.abs_y, el.w, el.h, skin_.button_border);
    if (!el.text.empty()) {
        DrawTextCentered(el.text, el.abs_x, el.abs_y, el.w, el.h, skin_.button_text.ToSDL());
    }
}

void UISystem::RenderLabel(const UIElement& el) {
    if (!el.text.empty()) {
        DrawText(el.text, el.abs_x, el.abs_y, skin_.label_text.ToSDL());
    }
}

void UISystem::RenderInput(const UIElement& el) {
    // Background + border
    DrawFilledRect(el.abs_x, el.abs_y, el.w, el.h, skin_.frame_bg);
    UIColor border = el.focused ? skin_.input_focus_border : skin_.frame_border;
    DrawRectBorder(el.abs_x, el.abs_y, el.w, el.h, border);

    // Draw value or placeholder
    const int PAD = 4;
    if (!el.value.empty()) {
        DrawText(el.value, el.abs_x + PAD, el.abs_y + (el.h - 14) / 2, skin_.button_text.ToSDL());
    } else if (!el.text.empty()) {
        DrawText(el.text, el.abs_x + PAD, el.abs_y + (el.h - 14) / 2, skin_.input_placeholder.ToSDL());
    }

    // Blinking cursor when focused
    if (el.focused && (SDL_GetTicks() % 1000 < 500) && font_) {
        // Measure text up to cursor position to find x offset
        std::string before_cursor = el.value.substr(0, static_cast<size_t>(el.cursor));
        int tw = 0, th = 0;
        TTF_SizeUTF8(font_, before_cursor.empty() ? " " : before_cursor.c_str(), &tw, &th);
        int cx = el.abs_x + PAD + (before_cursor.empty() ? 0 : tw);
        int cy = el.abs_y + 3;
        SDL_SetRenderDrawColor(renderer_, border.r, border.g, border.b, 200);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        SDL_RenderDrawLine(renderer_, cx, cy, cx, cy + el.h - 6);
    }
}

// ─── Draw helpers ─────────────────────────────────────────────────────────────

void UISystem::DrawFilledRect(int x, int y, int w, int h, UIColor c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(renderer_, &rect);
}

void UISystem::DrawRectBorder(int x, int y, int w, int h, UIColor c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(renderer_, &rect);
}

void UISystem::DrawText(const std::string& text, int x, int y, SDL_Color color) {
    if (!font_) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font_, text.c_str(), color);
    if (!surf) {
        std::fprintf(stderr, "UISystem::DrawText: TTF_RenderUTF8_Blended failed: %s\n", TTF_GetError());
        return;
    }
    // Convert to a known-good format for the renderer before creating texture
    SDL_Surface* conv = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(surf);
    if (!conv) {
        std::fprintf(stderr, "UISystem::DrawText: SDL_ConvertSurfaceFormat failed: %s\n", SDL_GetError());
        return;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, conv);
    SDL_FreeSurface(conv);
    if (!tex) {
        std::fprintf(stderr, "UISystem::DrawText: SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
        return;
    }
    // Explicitly set blend mode so alpha channel is used (required for Blended render)
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_Rect dst = {x, y, 0, 0};
    SDL_QueryTexture(tex, nullptr, nullptr, &dst.w, &dst.h);
    SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

void UISystem::DrawTextCentered(const std::string& text, int bx, int by,
                                int bw, int bh, SDL_Color color) {
    if (!font_) return;
    int tw = 0, th = 0;
    TTF_SizeUTF8(font_, text.c_str(), &tw, &th);
    int x = bx + (bw - tw) / 2;
    int y = by + (bh - th) / 2;
    DrawText(text, x, y, color);
}
