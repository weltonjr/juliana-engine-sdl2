#pragma once
#include <string>
#include <vector>

// Parsed content of a game's definition.toml.
// Loaded once at startup from the game directory passed on the command line.
struct GameDef {
    std::string id;
    std::string name;
    std::string version;
    std::string base_path;  // absolute or relative path to the game directory

    // [window]
    int window_width  = 1280;
    int window_height = 720;

    // [ui]
    std::string skin_path;   // relative to base_path — path to skin.toml
    std::string font_path;   // relative to base_path — path to .ttf (optional)
    int         font_size = 14;

    // [startup]
    std::string startup_script;  // relative to base_path — main Lua entry point

    // [packages] load = [...]
    std::vector<std::string> packages;  // content package dirs to load

    // Resolve a path relative to base_path
    std::string Resolve(const std::string& rel) const;
};
