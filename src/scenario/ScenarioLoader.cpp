#include "scenario/ScenarioLoader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

std::optional<ScenarioDef> ScenarioLoader::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::fprintf(stderr, "Failed to open scenario: %s\n", path.c_str());
        return std::nullopt;
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        std::fprintf(stderr, "JSON parse error in %s: %s\n", path.c_str(), e.what());
        return std::nullopt;
    }

    ScenarioDef scenario;

    // Scenario metadata
    if (j.contains("scenario")) {
        auto& s = j["scenario"];
        scenario.id = s.value("id", "unknown");
        scenario.name = s.value("name", "Unnamed");
        scenario.description = s.value("description", "");
        if (s.contains("packages")) {
            for (auto& p : s["packages"]) scenario.packages.push_back(p.get<std::string>());
        }
        if (s.contains("aspects")) {
            for (auto& a : s["aspects"]) scenario.aspects.push_back(a.get<std::string>());
        }
    }

    // Map config
    if (j.contains("map")) {
        auto& m = j["map"];
        scenario.map.width = m.value("width", 2048);
        scenario.map.height = m.value("height", 512);
        scenario.map.seed = m.value("seed", 0u);
        scenario.map.shape = m.value("shape", "flat");

        if (m.contains("shape_params")) {
            for (auto& [key, val] : m["shape_params"].items()) {
                scenario.map.shape_params.params[key] = val.get<float>();
            }
        }

        if (m.contains("materials")) {
            for (auto& mat : m["materials"]) {
                MaterialRule rule;
                rule.material_id = mat.value("id", "");
                rule.rule = mat.value("rule", "fill");
                rule.background_id = mat.value("background", "");
                rule.depth = mat.value("depth", 0);
                rule.min_depth = mat.value("min_depth", 0);
                scenario.map.materials.push_back(rule);
            }
        }

        if (m.contains("features")) {
            for (auto& feat : m["features"]) {
                FeatureConfig fc;
                fc.type = feat.value("type", "");
                fc.material = feat.value("material", "");
                fc.zone = feat.value("zone", "all");
                fc.density = feat.value("density", 0.05f);
                fc.count = feat.value("count", 0);
                fc.min_size = feat.value("min_size", 10);
                fc.max_size = feat.value("max_size", 40);
                fc.vein_radius = feat.value("vein_radius", 8);
                scenario.map.features.push_back(fc);
            }
        }
    }

    // Player slots
    if (j.contains("players") && j["players"].contains("slots")) {
        for (auto& slot_j : j["players"]["slots"]) {
            PlayerSlot slot;
            slot.type = slot_j.value("type", "required");
            slot.team = slot_j.value("team", 1);

            if (slot_j.contains("spawn")) {
                auto& sp = slot_j["spawn"];
                slot.spawn.zone = sp.value("zone", "surface");
                if (sp.contains("constraints")) {
                    auto& c = sp["constraints"];
                    slot.spawn.constraints.min_flat_width = c.value("min_flat_width", 32);
                    slot.spawn.constraints.min_sky_above = c.value("min_sky_above", 40);
                    slot.spawn.constraints.avoid_water = c.value("avoid_water", true);
                    slot.spawn.constraints.min_player_distance = c.value("min_player_distance", 200);
                }
            }

            if (slot_j.contains("objects")) {
                for (auto& obj : slot_j["objects"]) {
                    SpawnObject so;
                    so.definition = obj.value("definition", "");
                    so.count = obj.value("count", 1);
                    slot.objects.push_back(so);
                }
            }

            scenario.players.push_back(slot);
        }
    }

    std::printf("Loaded scenario: %s (%s)\n", scenario.name.c_str(), scenario.id.c_str());
    return scenario;
}
