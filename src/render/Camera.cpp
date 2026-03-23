#include "render/Camera.h"

Camera::Camera(int viewport_width, int viewport_height, float scale)
    : viewport_width_(viewport_width), viewport_height_(viewport_height), scale_(scale)
{
}

void Camera::Move(float dx, float dy) {
    x_ += dx;
    y_ += dy;
}

void Camera::SetPosition(float x, float y) {
    x_ = x;
    y_ = y;
}

void Camera::ClampToBounds(int terrain_width, int terrain_height) {
    int view_w = GetViewWorldWidth();
    int view_h = GetViewWorldHeight();
    x_ = std::max(0.0f, std::min(x_, static_cast<float>(terrain_width - view_w)));
    y_ = std::max(0.0f, std::min(y_, static_cast<float>(terrain_height - view_h)));
}

SDL_Rect Camera::GetSourceRect(int terrain_width, int terrain_height) const {
    int ix = static_cast<int>(x_);
    int iy = static_cast<int>(y_);
    int w = std::min(GetViewWorldWidth(), terrain_width - ix);
    int h = std::min(GetViewWorldHeight(), terrain_height - iy);
    return {ix, iy, w, h};
}

void Camera::WorldToScreen(float wx, float wy, int& sx, int& sy) const {
    sx = static_cast<int>((wx - x_) * scale_);
    sy = static_cast<int>((wy - y_) * scale_);
}

void Camera::ScreenToWorld(int sx, int sy, float& wx, float& wy) const {
    wx = x_ + sx / scale_;
    wy = y_ + sy / scale_;
}
