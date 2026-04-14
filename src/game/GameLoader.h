#pragma once
#include "game/GameDef.h"
#include <string>

class GameLoader {
public:
    // Load game definition from a directory path.
    // Reads <game_dir>/definition.toml and returns the parsed GameDef.
    static GameDef Load(const std::string& game_dir);
};
