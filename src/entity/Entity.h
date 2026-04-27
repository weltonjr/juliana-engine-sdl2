#pragma once

#include "core/Types.h"
#include "package/ObjectDef.h"
#include <string>
#include <unordered_map>

struct Entity {
    EntityID id = 0;
    const ObjectDef* definition = nullptr;

    // Position & velocity (float; FixedPoint.h preserved for future network determinism)
    float pos_x = 0.0f, pos_y = 0.0f;
    float vel_x = 0.0f, vel_y = 0.0f;

    // Previous position (for render interpolation)
    float prev_pos_x = 0.0f, prev_pos_y = 0.0f;

    // Physics
    int width = 12;
    int height = 20;
    float mass = 50.0f;
    float max_fall_speed = 400.0f;
    int step_up = 4;
    bool solid = false;
    bool on_ground = false;
    bool was_airborne = false;

    // Rotation state (synced from Box2D when definition->rotation == true)
    float angle_rad = 0.0f;
    float angular_velocity = 0.0f;

    // Action state
    std::string current_action = "Idle";
    int action_frame = 0;
    int action_timer = 0;

    // Facing direction: 1 = right, -1 = left
    int facing = 1;

    // Dig state
    int dig_dir_x = 0;  // -1, 0, 1
    int dig_dir_y = 0;  // -1, 0, 1
    int dig_timer = 0;
    int dig_radius = 12;

    // Owner player slot (0 = unowned)
    uint32_t owner_slot = 0;

    // Per-instance mutable properties (seeded from ObjectDef::properties at spawn).
    // Use these for runtime values like HP that vary per entity.
    std::unordered_map<std::string, float> instance_properties;

    // Alive flag
    bool alive = true;
    bool pending_destroy = false;
};
