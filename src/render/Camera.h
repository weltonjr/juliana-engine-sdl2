#pragma once

#include <SDL2/SDL.h>
#include <algorithm>

class Camera {
public:
    Camera(int viewport_width, int viewport_height, float scale = 2.0f);

    void Move(float dx, float dy);
    void SetPosition(float x, float y);
    void ClampToBounds(int terrain_width, int terrain_height);

    SDL_Rect GetSourceRect(int terrain_width, int terrain_height) const;

    float GetX() const { return x_; }
    float GetY() const { return y_; }
    int GetViewportWidth() const { return viewport_width_; }
    int GetViewportHeight() const { return viewport_height_; }
    float GetScale() const { return scale_; }
    void  SetScale(float s) { scale_ = std::max(0.25f, std::min(s, 16.0f)); }

    // How many terrain pixels are visible
    int GetViewWorldWidth() const { return static_cast<int>(viewport_width_ / scale_); }
    int GetViewWorldHeight() const { return static_cast<int>(viewport_height_ / scale_); }

    void WorldToScreen(float wx, float wy, int& sx, int& sy) const;
    void ScreenToWorld(int sx, int sy, float& wx, float& wy) const;

private:
    float x_ = 0.0f;
    float y_ = 0.0f;
    int viewport_width_;
    int viewport_height_;
    float scale_;
};
