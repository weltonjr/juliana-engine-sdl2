#include "ui/UIElement.h"

std::shared_ptr<UIElement> UIElement::AddFrame(int px, int py, int pw, int ph) {
    auto el = std::make_shared<UIElement>();
    el->type = UIElementType::Frame;
    el->x = px; el->y = py; el->w = pw; el->h = ph;
    children.push_back(el);
    return el;
}

std::shared_ptr<UIElement> UIElement::AddButton(const std::string& label, int px, int py, int pw, int ph) {
    auto el = std::make_shared<UIElement>();
    el->type = UIElementType::Button;
    el->text = label;
    el->x = px; el->y = py; el->w = pw; el->h = ph;
    children.push_back(el);
    return el;
}

std::shared_ptr<UIElement> UIElement::AddLabel(const std::string& label, int px, int py) {
    auto el = std::make_shared<UIElement>();
    el->type = UIElementType::Label;
    el->text = label;
    el->x = px; el->y = py;
    children.push_back(el);
    return el;
}

std::shared_ptr<UIElement> UIElement::AddInput(const std::string& placeholder, int px, int py, int pw, int ph) {
    auto el = std::make_shared<UIElement>();
    el->type = UIElementType::Input;
    el->text = placeholder;  // used as placeholder when value is empty
    el->x = px; el->y = py; el->w = pw; el->h = ph;
    children.push_back(el);
    return el;
}
