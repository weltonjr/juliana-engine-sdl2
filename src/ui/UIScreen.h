#pragma once
#include "ui/UIElement.h"
#include <string>
#include <memory>

// A UIScreen is a named container pushed onto UISystem's screen stack.
// It owns a single implicit `root` Frame at the screen origin; all user
// widgets live under it, which lets UISystem traverse any screen through
// a single pointer instead of a separate root_elements vector.
struct UIScreen {
    std::string                name;
    std::shared_ptr<UIElement> root;

    UIScreen() : root(std::make_shared<UIElement>()) {
        root->type = UIElementType::Frame;
    }

    // Factory helpers forward to the root Frame.
    std::shared_ptr<UIElement> AddFrame (int x, int y, int w, int h) {
        return root->AddFrame(x, y, w, h);
    }
    std::shared_ptr<UIElement> AddButton(const std::string& text, int x, int y, int w, int h) {
        return root->AddButton(text, x, y, w, h);
    }
    std::shared_ptr<UIElement> AddLabel(const std::string& text, int x, int y) {
        return root->AddLabel(text, x, y);
    }
    std::shared_ptr<UIElement> AddInput(const std::string& placeholder, int x, int y, int w, int h) {
        return root->AddInput(placeholder, x, y, w, h);
    }
};
