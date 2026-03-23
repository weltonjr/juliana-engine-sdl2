#pragma once

#include <SDL2/SDL.h>

class Window {
public:
    Window(const char* title, int width, int height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    SDL_Renderer* GetRenderer() const { return renderer_; }
    SDL_Window* GetWindow() const { return window_; }
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    int width_;
    int height_;
};
