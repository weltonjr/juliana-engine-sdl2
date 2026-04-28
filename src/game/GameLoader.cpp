#include "game/GameLoader.h"
#include "game/GameDef.h"
#include <toml++/toml.hpp>
#include <filesystem>
#include <cstdio>

namespace fs = std::filesystem;

std::string GameDef::Resolve(const std::string& rel) const {
    if (rel.empty()) return "";
    return base_path + "/" + rel;
}

GameDef GameLoader::Load(const std::string& game_dir) {
    GameDef def;
    def.base_path = game_dir;

    std::string toml_path = game_dir + "/definition.toml";

    if (!fs::exists(toml_path)) {
        std::fprintf(stderr, "GameLoader: definition.toml not found at '%s'\n", toml_path.c_str());
        def.name = "Unknown Game";
        return def;
    }

    try {
        auto tbl = toml::parse_file(toml_path);

        def.id      = tbl["game"]["id"]     .value_or(std::string(""));
        def.name    = tbl["game"]["name"]   .value_or(std::string("Unnamed Game"));
        def.version = tbl["game"]["version"].value_or(std::string("0.1.0"));

        def.window_width  = static_cast<int>(tbl["window"]["width"] .value_or(int64_t(1280)));
        def.window_height = static_cast<int>(tbl["window"]["height"].value_or(int64_t(720)));

        def.ui_debugger = tbl["ui"]["debugger"].value_or(false);

        def.startup_script = tbl["startup"]["script"].value_or(std::string(""));

        if (auto load_arr = tbl["packages"]["load"].as_array()) {
            for (auto& item : *load_arr) {
                if (auto s = item.value<std::string>())
                    def.packages.push_back(*s);
            }
        }

    } catch (const toml::parse_error& e) {
        std::fprintf(stderr, "GameLoader: TOML parse error in '%s': %s\n", toml_path.c_str(), e.what());
    }

    std::printf("Loaded game: %s v%s\n", def.name.c_str(), def.version.c_str());
    return def;
}
