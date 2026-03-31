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

    // Action state
    std::string current_action = "Idle";
    int action_frame = 0;
    int action_timer = 0;

    // Facing direction: 1 = right, -1 = left
    int facing = 1;

    // Dig state
    int dig_dir_x = 0;  // -1, 0, 1
    int dig_dir_y = 0;  // -1, 0, 1
    float dig_progress = 0.0f;        // fractional pixel accumulator
    int dig_size_w = 12;              // dig area width (defaults to character width)
    int dig_size_h = 20;              // dig area height (defaults to character height)
    float dig_speed = 5.0f;           // pixels per second

    // Per-material dig progress: tracks pixels dug per material for spawn intervals
    std::unordered_map<MaterialID, float> dig_material_progress;

    // Owner player slot (0 = unowned)
    uint32_t owner_slot = 0;

    // Alive flag
    bool alive = true;
    bool pending_destroy = false;
};
