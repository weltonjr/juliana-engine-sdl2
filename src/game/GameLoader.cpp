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

        if (auto game = tbl["game"].as_table()) {
            def.id      = game->get_as<std::string>("id").value_or("");
            def.name    = game->get_as<std::string>("name").value_or("Unnamed Game");
            def.version = game->get_as<std::string>("version").value_or("0.1.0");
        }

        if (auto window = tbl["window"].as_table()) {
            def.window_width  = static_cast<int>(window->get_as<int64_t>("width") .value_or(1280));
            def.window_height = static_cast<int>(window->get_as<int64_t>("height").value_or(720));
        }

        if (auto ui = tbl["ui"].as_table()) {
            def.skin_path = ui->get_as<std::string>("skin").value_or("");
            def.font_path = ui->get_as<std::string>("font").value_or("");
            def.font_size = static_cast<int>(ui->get_as<int64_t>("font_size").value_or(14));
        }

        if (auto startup = tbl["startup"].as_table()) {
            def.startup_script = startup->get_as<std::string>("script").value_or("");
        }

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
