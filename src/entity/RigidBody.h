#pragma once
#include "raylib.h"
#include "../core/Types.h"

class TerrainFacade;

// Base physics object: AABB position/velocity with terrain collision.
// Resolves X and Y axes separately to avoid corner-sticking.
struct RigidBody {
    Vector2 position     = {0, 0}; // top-left corner in world pixels
    Vector2 velocity     = {0, 0}; // pixels per second
    Vector2 size         = {12, 20}; // width × height in pixels
    float   gravity_scale = 1.0f;
    bool    on_ground    = false;

    // Integrate physics and resolve terrain collision for one fixed timestep.
    void update(float dt, const TerrainFacade& terrain);

    // Convenience: AABB right and bottom edges
    float right()  const { return position.x + size.x; }
    float bottom() const { return position.y + size.y; }

private:
    void resolve_x(const TerrainFacade& terrain);
    void resolve_y(const TerrainFacade& terrain);
};
