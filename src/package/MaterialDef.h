#pragma once

#include "core/Types.h"
#include <string>

enum class MaterialState {
    None,
    Solid,
    Powder,
    Liquid,
    Gas
};

struct MaterialDef {
    std::string id;
    std::string name;
    std::string qualified_id;  // "package:id"

    // Physics
    MaterialState state = MaterialState::None;
    int density = 100;
    float friction = 0.8f;
    int hardness = 30;

    // Visual
    Color color;
    int color_variation = 0;
    float transparency = 0.0f;
    bool glow = false;

    // Behavior
    bool gravity = false;
    bool flammable = false;
    int blast_resistance = 20;
    int flow_rate = 0;
    float liquid_drag = 0.0f;
    int rise_rate = 0;
    int dispersion = 0;
    int lifetime = 0;
    std::string dig_product;
    std::string small_fragment;
    int min_fragment_pixels = 8;

    // Runtime assigned
    MaterialID runtime_id = 0;
};
