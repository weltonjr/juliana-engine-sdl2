#pragma once

#include "core/Types.h"
#include "core/FixedPoint.h"
#include "package/ObjectDef.h"
#include <string>

struct Entity {
    EntityID id = 0;
    const ObjectDef* definition = nullptr;

    // Position & velocity (fixed-point for determinism)
    Fixed pos_x, pos_y;
    Fixed vel_x, vel_y;

    // Previous position (for render interpolation)
    Fixed prev_pos_x, prev_pos_y;

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
    int dig_timer = 0;
    int dig_radius = 12;

    // Owner player slot (0 = unowned)
    uint32_t owner_slot = 0;

    // Alive flag
    bool alive = true;
    bool pending_destroy = false;
};
