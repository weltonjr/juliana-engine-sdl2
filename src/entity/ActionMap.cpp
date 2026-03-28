#include "entity/ActionMap.h"
#include <toml++/toml.hpp>
#include <cstdio>

bool ActionMap::LoadFromFile(const std::string& file_path) {
    try {
        auto tbl = toml::parse_file(file_path);
        auto* actions = tbl["actions"].as_table();
        if (!actions) return false;

        for (auto& [name, val] : *actions) {
            auto* action_tbl = val.as_table();
            if (!action_tbl) continue;

            ActionDef def;
            def.row = (*action_tbl)["row"].value_or(0);
            def.frames = (*action_tbl)["frames"].value_or(1);
            def.delay = (*action_tbl)["delay"].value_or(1);
            def.next = (*action_tbl)["next"].value_or<std::string>("");
            def.length = (*action_tbl)["length"].value_or(0);
            def.procedure = (*action_tbl)["procedure"].value_or<std::string>("none");

            actions_[std::string(name.str())] = def;
        }

        std::printf("  Loaded %zu actions from %s\n", actions_.size(), file_path.c_str());
        return true;
    } catch (const toml::parse_error& e) {
        std::fprintf(stderr, "Failed to parse animations: %s: %s\n", file_path.c_str(), e.what());
        return false;
    }
}

const ActionDef* ActionMap::GetAction(const std::string& name) const {
    auto it = actions_.find(name);
    return it != actions_.end() ? &it->second : nullptr;
}

bool ActionMap::HasAction(const std::string& name) const {
    return actions_.count(name) > 0;
}
