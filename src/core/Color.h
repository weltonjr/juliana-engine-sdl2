#pragma once
#include <cstdint>

struct Color { uint8_t r, g, b, a; };

static constexpr Color WHITE     = {255, 255, 255, 255};
static constexpr Color BLACK     = {0,   0,   0,   255};
static constexpr Color YELLOW    = {255, 235, 0,   255};
static constexpr Color RED       = {255, 0,   0,   255};
static constexpr Color GREEN     = {0,   255, 0,   255};
static constexpr Color BLANK     = {0,   0,   0,   0  };
static constexpr Color GRAY      = {128, 128, 128, 255};
static constexpr Color LIGHTGRAY = {200, 200, 200, 255};
static constexpr Color SKYBLUE   = {102, 191, 255, 255};
