#include "ui/UISkin.h"
#include <toml++/toml.hpp>
#include <filesystem>
#include <cstdio>

namespace fs = std::filesystem;

static UIColor ReadColor(const toml::table& tbl, const char* key, UIColor def) {
    auto node = tbl[key];
    if (auto arr = node.as_array(); arr && arr->size() >= 3) {
        UIColor c;
        c.r = static_cast<uint8_t>(arr->get_as<int64_t>(0)->get());
        c.g = static_cast<uint8_t>(arr->get_as<int64_t>(1)->get());
        c.b = static_cast<uint8_t>(arr->get_as<int64_t>(2)->get());
        c.a = arr->size() >= 4 ? static_cast<uint8_t>(arr->get_as<int64_t>(3)->get()) : 255;
        return c;
    }
    return def;
}

UISkin UISkin::LoadFromFile(const std::string& path) {
    UISkin skin;  // starts with defaults

    if (!fs::exists(path)) {
        std::printf("UISkin: '%s' not found, using defaults\n", path.c_str());
        return skin;
    }

    try {
        auto tbl = toml::parse_file(path);

        if (auto colors = tbl["colors"].as_table()) {
            skin.frame_bg      = ReadColor(*colors, "frame_bg",      skin.frame_bg);
            skin.frame_border  = ReadColor(*colors, "frame_border",  skin.frame_border);
            skin.button_normal = ReadColor(*colors, "button_normal", skin.button_normal);
            skin.button_hover  = ReadColor(*colors, "button_hover",  skin.button_hover);
            skin.button_pressed= ReadColor(*colors, "button_pressed",skin.button_pressed);
            skin.button_border = ReadColor(*colors, "button_border", skin.button_border);
            skin.button_text   = ReadColor(*colors, "button_text",   skin.button_text);
            skin.label_text    = ReadColor(*colors, "label_text",    skin.label_text);
        }
    } catch (const toml::parse_error& e) {
        std::fprintf(stderr, "UISkin: parse error in '%s': %s\n", path.c_str(), e.what());
    }

    return skin;
}
