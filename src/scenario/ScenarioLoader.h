#pragma once

#include "scenario/ScenarioDef.h"
#include <string>
#include <optional>

class ScenarioLoader {
public:
    static std::optional<ScenarioDef> LoadFromFile(const std::string& path);
};
