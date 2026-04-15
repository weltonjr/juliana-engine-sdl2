#include "package/PackageLoader.h"
#include <toml++/toml.hpp>
#include <filesystem>
#include <cstdio>

namespace fs = std::filesystem;

PackageLoader::PackageLoader(DefinitionRegistry& registry)
    : registry_(registry)
{
}

void PackageLoader::LoadAll(const std::string& packages_dir) {
    if (!fs::exists(packages_dir)) {
        std::printf("Warning: packages directory '%s' not found\n", packages_dir.c_str());
        return;
    }

    // Check if the path itself is a package root (has definition.toml with [package])
    auto root_def = fs::path(packages_dir) / "definition.toml";
    if (fs::exists(root_def)) {
        try {
            auto tbl = toml::parse_file(root_def.string());
            if (tbl.contains("package")) {
                auto pkg_id = tbl["package"]["id"].value_or<std::string>("");
                if (!pkg_id.empty()) {
                    std::printf("Loading package: %s\n", pkg_id.c_str());
                    LoadPackage(packages_dir, pkg_id);
                    return;  // Direct package path — don't scan children as packages
                }
            }
        } catch (const toml::parse_error& e) {
            std::fprintf(stderr, "TOML parse error in %s: %s\n", root_def.c_str(), e.what());
        }
    }

    // Otherwise scan subdirectories for packages
    for (const auto& entry : fs::directory_iterator(packages_dir)) {
        if (!entry.is_directory()) continue;

        auto def_path = entry.path() / "definition.toml";
        if (!fs::exists(def_path)) continue;

        try {
            auto tbl = toml::parse_file(def_path.string());
            if (tbl.contains("package")) {
                auto pkg_id = tbl["package"]["id"].value_or<std::string>("");
                if (!pkg_id.empty()) {
                    std::printf("Loading package: %s\n", pkg_id.c_str());
                    LoadPackage(entry.path().string(), pkg_id);
                }
            }
        } catch (const toml::parse_error& e) {
            std::fprintf(stderr, "TOML parse error in %s: %s\n", def_path.c_str(), e.what());
        }
    }
}

void PackageLoader::LoadPackage(const std::string& package_dir, const std::string& package_id) {
    // Recursively scan for definition.toml files
    for (const auto& entry : fs::recursive_directory_iterator(package_dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().filename() != "definition.toml") continue;
        if (entry.path().parent_path().string() == package_dir) continue; // skip package root

        LoadDefinition(entry.path().parent_path().string(), package_id);
    }
}

void PackageLoader::LoadDefinition(const std::string& dir_path, const std::string& package_id) {
    auto file_path = dir_path + "/definition.toml";
    try {
        auto tbl = toml::parse_file(file_path);

        if (tbl.contains("material")) {
            ParseMaterial(file_path, dir_path, package_id);
        } else if (tbl.contains("background")) {
            ParseBackground(file_path, package_id);
        } else if (tbl.contains("procedure")) {
            ParseProcedure(file_path, dir_path, package_id);
        } else if (tbl.contains("object")) {
            ParseObject(file_path, dir_path, package_id);
        } else if (tbl.contains("aspect")) {
            // Aspects are loaded as part of object loading (inline) or standalone
            // Standalone aspects will be loaded when referenced
        }
    } catch (const toml::parse_error& e) {
        std::fprintf(stderr, "TOML parse error in %s: %s\n", file_path.c_str(), e.what());
    }
}

void PackageLoader::ParseMaterial(const std::string& file_path, const std::string& dir_path, const std::string& package_id) {
    auto tbl = toml::parse_file(file_path);
    auto def = std::make_unique<MaterialDef>();

    auto& mat = *tbl["material"].as_table();
    def->id = mat["id"].value_or<std::string>("");
    def->name = mat["name"].value_or(std::string(def->id));
    def->qualified_id = package_id + ":" + def->id;

    // Physics
    if (tbl.contains("physics")) {
        auto& phys = *tbl["physics"].as_table();
        auto state_str = phys["state"].value_or<std::string>("none");
        if (state_str == "solid") def->state = MaterialState::Solid;
        else if (state_str == "powder") def->state = MaterialState::Powder;
        else if (state_str == "liquid") def->state = MaterialState::Liquid;
        else if (state_str == "gas") def->state = MaterialState::Gas;
        else def->state = MaterialState::None;

        def->density = phys["density"].value_or(100);
        def->friction = phys["friction"].value_or(0.8f);
        def->hardness = phys["hardness"].value_or(30);
    }

    // Visual
    if (tbl.contains("visual")) {
        auto& vis = *tbl["visual"].as_table();
        if (auto arr = vis["color"].as_array(); arr && arr->size() >= 3) {
            def->color.r = static_cast<uint8_t>((*arr)[0].value_or(0));
            def->color.g = static_cast<uint8_t>((*arr)[1].value_or(0));
            def->color.b = static_cast<uint8_t>((*arr)[2].value_or(0));
        }
        def->color_variation = vis["color_variation"].value_or(0);
        def->transparency = vis["transparency"].value_or(0.0f);
        def->glow = vis["glow"].value_or(false);
    }

    // Behavior
    if (tbl.contains("behavior")) {
        auto& beh = *tbl["behavior"].as_table();
        def->gravity = beh["gravity"].value_or(false);
        def->flammable = beh["flammable"].value_or(false);
        def->blast_resistance = beh["blast_resistance"].value_or(20);
        def->flow_rate = beh["flow_rate"].value_or(0);
        def->liquid_drag = beh["liquid_drag"].value_or(0.0f);
        def->inertial_resistance = beh["inertial_resistance"].value_or(0.0f);
        def->rise_rate = beh["rise_rate"].value_or(0);
        def->dispersion = beh["dispersion"].value_or(0);
        def->lifetime = beh["lifetime"].value_or(0);
        def->dig_product = beh["dig_product"].value_or<std::string>("");
        def->small_fragment = beh["small_fragment"].value_or<std::string>("");
        def->min_fragment_pixels = beh["min_fragment_pixels"].value_or(8);
    }

    // Check for script
    auto script_path = dir_path + "/script.lua";
    if (fs::exists(script_path)) {
        // Store for future use
    }

    std::printf("  Material: %s (color: %d,%d,%d)\n",
        def->qualified_id.c_str(), def->color.r, def->color.g, def->color.b);
    registry_.RegisterMaterial(std::move(def));
}

void PackageLoader::ParseBackground(const std::string& file_path, const std::string& package_id) {
    auto tbl = toml::parse_file(file_path);
    auto def = std::make_unique<BackgroundDef>();

    auto& bg = *tbl["background"].as_table();
    def->id = bg["id"].value_or<std::string>("");
    def->name = bg["name"].value_or(std::string(def->id));
    def->qualified_id = package_id + ":" + def->id;

    if (tbl.contains("visual")) {
        auto& vis = *tbl["visual"].as_table();
        if (auto arr = vis["color"].as_array(); arr && arr->size() >= 3) {
            def->color.r = static_cast<uint8_t>((*arr)[0].value_or(0));
            def->color.g = static_cast<uint8_t>((*arr)[1].value_or(0));
            def->color.b = static_cast<uint8_t>((*arr)[2].value_or(0));
        }
        def->color_variation = vis["color_variation"].value_or(0);
        def->transparent = vis["transparent"].value_or(false);
    }

    std::printf("  Background: %s\n", def->qualified_id.c_str());
    registry_.RegisterBackground(std::move(def));
}

void PackageLoader::ParseProcedure(const std::string& file_path, const std::string& dir_path, const std::string& package_id) {
    auto tbl = toml::parse_file(file_path);
    auto def = std::make_unique<ProcedureDef>();

    auto& proc = *tbl["procedure"].as_table();
    def->id = proc["id"].value_or<std::string>("");
    def->name = proc["name"].value_or(std::string(def->id));
    def->description = proc["description"].value_or<std::string>("");
    def->qualified_id = package_id + ":" + def->id;
    def->engine_impl = proc["engine_impl"].value_or<std::string>("");

    auto script_path = dir_path + "/script.lua";
    if (fs::exists(script_path)) {
        def->script_path = script_path;
    }

    std::printf("  Procedure: %s (engine_impl: %s)\n",
        def->qualified_id.c_str(),
        def->engine_impl.empty() ? "none" : def->engine_impl.c_str());
    registry_.RegisterProcedure(std::move(def));
}

void PackageLoader::ParseObject(const std::string& file_path, const std::string& dir_path, const std::string& package_id) {
    auto tbl = toml::parse_file(file_path);
    auto def = std::make_unique<ObjectDef>();

    auto& obj = *tbl["object"].as_table();
    def->id = obj["id"].value_or<std::string>("");
    def->name = obj["name"].value_or(std::string(def->id));
    def->qualified_id = package_id + ":" + def->id;

    if (auto cats = obj["category"].as_array()) {
        for (auto& cat : *cats) {
            def->categories.push_back(cat.value_or<std::string>(""));
        }
    }
    if (auto aspects = obj["aspects"].as_array()) {
        for (auto& asp : *aspects) {
            def->aspects.push_back(asp.value_or<std::string>(""));
        }
    }
    def->player_controllable = obj["player_controllable"].value_or(false);

    // Physics
    if (tbl.contains("physics")) {
        auto& phys = *tbl["physics"].as_table();
        auto mode_str = phys["mode"].value_or<std::string>("dynamic");
        if (mode_str == "dynamic") def->physics_mode = PhysicsMode::Dynamic;
        else if (mode_str == "static") def->physics_mode = PhysicsMode::Static;
        else if (mode_str == "kinematic") def->physics_mode = PhysicsMode::Kinematic;

        def->mass = phys["mass"].value_or(50.0f);
        def->max_fall_speed = phys["max_fall_speed"].value_or(400.0f);
        def->solid = phys["solid"].value_or(false);
        def->overlap_detection = phys["overlap_detection"].value_or(false);
        def->step_up = phys["step_up"].value_or(4);
        def->rotation = phys["rotation"].value_or(false);
        def->angular_drag = phys["angular_drag"].value_or(0.98f);
        def->bounce_angular_transfer = phys["bounce_angular_transfer"].value_or(0.3f);

        if (auto sz = phys["size"].as_array(); sz && sz->size() >= 2) {
            def->size_w = (*sz)[0].value_or(12);
            def->size_h = (*sz)[1].value_or(20);
        }
    }

    // Points
    if (tbl.contains("points")) {
        for (auto& [key, val] : *tbl["points"].as_table()) {
            if (auto arr = val.as_array(); arr && arr->size() >= 2) {
                def->points[std::string(key.str())] = {
                    (*arr)[0].value_or(0),
                    (*arr)[1].value_or(0)
                };
            }
        }
    }

    // Container
    if (tbl.contains("container")) {
        auto& cont = *tbl["container"].as_table();
        def->container_enabled = cont["enabled"].value_or(false);
        def->container_max_slots = cont["max_slots"].value_or(0);
        if (auto filt = cont["filter"].as_array()) {
            for (auto& f : *filt) {
                def->container_filter.push_back(f.value_or<std::string>(""));
            }
        }
    }

    // Editor / visual color
    if (tbl.contains("visual")) {
        auto& vis = *tbl["visual"].as_table();
        if (auto arr = vis["color"].as_array(); arr && arr->size() >= 3) {
            def->color.r = static_cast<uint8_t>((*arr)[0].value_or(200));
            def->color.g = static_cast<uint8_t>((*arr)[1].value_or(200));
            def->color.b = static_cast<uint8_t>((*arr)[2].value_or(200));
        }
    }

    // Properties
    if (tbl.contains("properties")) {
        for (auto& [key, val] : *tbl["properties"].as_table()) {
            def->properties[std::string(key.str())] = val.value_or(0.0f);
        }
    }

    // File paths
    auto script = dir_path + "/script.lua";
    if (fs::exists(script)) def->script_path = script;
    auto sprite = dir_path + "/sprite.png";
    if (fs::exists(sprite)) def->sprite_path = sprite;
    auto anims = dir_path + "/animations.toml";
    if (fs::exists(anims)) def->animations_path = anims;

    std::printf("  Object: %s (%dx%d)\n",
        def->qualified_id.c_str(), def->size_w, def->size_h);
    registry_.RegisterObject(std::move(def));
}
