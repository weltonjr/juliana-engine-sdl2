#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>

enum class UIElementType { Frame, Button, Label, Image, Input };

// A single UI element. Elements form a tree: Frames can contain any children.
// Positions are relative to the parent element (or screen origin for root elements).
// abs_x / abs_y are computed by UISystem before each render pass.
struct UIElement {
    UIElementType type = UIElementType::Frame;
    std::string   id;

    // Layout (relative to parent)
    int  x = 0, y = 0, w = 0, h = 0;
    bool visible = true;

    // Content (used by Button and Label)
    std::string text;
    bool        text_left = false;  // if true, button text is left-aligned with padding
    bool        disabled  = false;  // if true, input/button is non-interactive and grayed out

    // Button interaction
    std::function<void()> on_click;
    bool hovered = false;
    bool pressed  = false;

    // Input element state
    std::string value;           // current text content
    int         cursor = 0;      // insertion point (character index)
    bool        focused = false;
    int         max_length = 256;
    std::function<void(const std::string&)> on_change;

    // Children (Frames can hold any element type)
    std::vector<std::shared_ptr<UIElement>> children;

    // Computed absolute position (filled by UISystem::ComputeAbsPositions)
    int abs_x = 0, abs_y = 0;

    // --- Factory helpers (add a child and return it) ---
    std::shared_ptr<UIElement> AddFrame (int x, int y, int w, int h);
    std::shared_ptr<UIElement> AddButton(const std::string& text, int x, int y, int w, int h);
    std::shared_ptr<UIElement> AddLabel (const std::string& text, int x, int y);
    std::shared_ptr<UIElement> AddInput (const std::string& placeholder, int x, int y, int w, int h);
};
