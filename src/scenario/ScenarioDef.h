#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

// Map generation config
struct MapShapeParams {
    std::unordered_map<std::string, float> params;
    float Get(const std::string& key, float default_val = 0.0f) const {
        auto it = params.find(key);
        return it != params.end() ? it->second : default_val;
    }
};

struct MaterialRule {
    std::string material_id;       // "base:Dirt"
    std::string rule;              // "surface_layer", "deep", "fill", etc.
    std::string background_id;     // "base:DirtWall"
    int depth = 0;
    int min_depth = 0;
};

struct FeatureConfig {
    std::string type;              // "caves", "ore_veins", "lakes", etc.
    std::string material;          // for ore_veins
    std::string zone;              // "surface", "underground", "deep", "rock", "all"
    float density = 0.05f;
    int count = 0;
    int min_size = 10;
    int max_size = 40;
    int vein_radius = 8;
};

struct MapConfig {
    int width = 2048;
    int height = 512;
    uint32_t seed = 0;             // 0 = random
    std::string shape = "flat";
    MapShapeParams shape_params;
    std::vector<MaterialRule> materials;
    std::vector<FeatureConfig> features;
};

struct SpawnConstraints {
    int min_flat_width = 32;
    int min_sky_above = 40;
    bool avoid_water = true;
    int min_player_distance = 200;
};

struct SpawnConfig {
    std::string zone = "surface";
    SpawnConstraints constraints;
};

struct SpawnObject {
    std::string definition;
    int count = 1;
};

struct PlayerSlot {
    std::string type = "required";  // "required", "optional", "ai", "none"
    int team = 1;
    SpawnConfig spawn;
    std::vector<SpawnObject> objects;
};

struct CellOverride {
    int x = 0, y = 0;
    std::string material_id;    // qualified id, e.g. "base:Rock"
    std::string background_id;  // may be empty
};

struct ScenarioDef {
    std::string id;
    std::string name;
    std::string description;
    std::string icon;           // path relative to scenario folder, e.g. "icon.png"
    std::vector<std::string> packages;
    std::vector<std::string> aspects;

    MapConfig map;
    std::vector<PlayerSlot> players;
    std::vector<CellOverride> overrides;  // manual cell edits applied after generation
};
