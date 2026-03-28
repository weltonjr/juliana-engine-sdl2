#pragma once

#include <string>
#include <unordered_map>

struct ActionDef {
    int row = 0;
    int frames = 1;
    int delay = 1;
    std::string next;
    int length = 0;        // 0 = indefinite
    std::string procedure; // procedure ID (unqualified)
};

class ActionMap {
public:
    bool LoadFromFile(const std::string& file_path);

    const ActionDef* GetAction(const std::string& name) const;
    bool HasAction(const std::string& name) const;

private:
    std::unordered_map<std::string, ActionDef> actions_;
};
