#include "ui/UIScreen.h"

std::shared_ptr<UIElement> UIScreen::AddFrame(int x, int y, int w, int h) {
    auto el = std::make_shared<UIElement>();
    el->type = UIElementType::Frame;
    el->x = x; el->y = y; el->w = w; el->h = h;
    root_elements.push_back(el);
    return el;
}

std::shared_ptr<UIElement> UIScreen::AddButton(const std::string& text, int x, int y, int w, int h) {
    auto el = std::make_shared<UIElement>();
    el->type = UIElementType::Button;
    el->text = text;
    el->x = x; el->y = y; el->w = w; el->h = h;
    root_elements.push_back(el);
    return el;
}

std::shared_ptr<UIElement> UIScreen::AddLabel(const std::string& text, int x, int y) {
    auto el = std::make_shared<UIElement>();
    el->type = UIElementType::Label;
    el->text = text;
    el->x = x; el->y = y;
    root_elements.push_back(el);
    return el;
}
