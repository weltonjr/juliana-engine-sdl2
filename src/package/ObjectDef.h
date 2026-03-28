#pragma once

#include "core/Types.h"
#include <string>
#include <vector>
#include <unordered_map>

enum class PhysicsMode {
    Dynamic,
    Static,
    Kinematic
};

struct ObjectDef {
    std::string id;
    std::string name;
    std::string qualified_id;
    std::vector<std::string> categories;
    std::vector<std::string> aspects;
    bool player_controllable = false;  // if true, player input is routed to instances of this object

    // Physics
    PhysicsMode physics_mode = PhysicsMode::Dynamic;
    float mass = 50.0f;
    float max_fall_speed = 400.0f;
    int size_w = 12;
    int size_h = 20;
    bool rotation = false;
    float angular_drag = 0.98f;
    float bounce_angular_transfer = 0.3f;
    bool solid = false;
    bool overlap_detection = false;
    int step_up = 4;

    // Named points
    std::unordered_map<std::string, std::pair<int, int>> points;

    // Container
    bool container_enabled = false;
    int container_max_slots = 0;
    std::vector<std::string> container_filter;

    // Properties (arbitrary key-values)
    std::unordered_map<std::string, float> properties;

    // Rendering
    Color color{200, 200, 200};  // fallback color when no sprite

    // File paths
    std::string script_path;
    std::string sprite_path;
    std::string animations_path;
};
