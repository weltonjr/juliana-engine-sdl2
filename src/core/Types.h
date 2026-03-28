#pragma once

#include <cstdint>

using EntityID = uint32_t;
using MaterialID = uint8_t;
using BackgroundID = uint8_t;

struct Color {
    uint8_t r, g, b, a;

    Color() : r(0), g(0), b(0), a(255) {}
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}
};
