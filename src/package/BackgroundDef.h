#pragma once

#include "core/Types.h"
#include <string>

struct BackgroundDef {
    std::string id;
    std::string name;
    std::string qualified_id;

    Color color;
    int color_variation = 0;
    bool transparent = false;

    BackgroundID runtime_id = 0;
};
