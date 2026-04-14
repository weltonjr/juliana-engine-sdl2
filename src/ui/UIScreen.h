#pragma once
#include "ui/UIElement.h"
#include <string>
#include <vector>
#include <memory>

// Top-level container pushed onto the UISystem screen stack.
// Root elements are positioned relative to the screen origin (top-left = 0,0).
struct UIScreen {
    std::string name;
    std::vector<std::shared_ptr<UIElement>> root_elements;

    // --- Factory helpers (add a root element and return it) ---
    std::shared_ptr<UIElement> AddFrame (int x, int y, int w, int h);
    std::shared_ptr<UIElement> AddButton(const std::string& text, int x, int y, int w, int h);
    std::shared_ptr<UIElement> AddLabel (const std::string& text, int x, int y);
    std::shared_ptr<UIElement> AddInput (const std::string& placeholder, int x, int y, int w, int h);
};
