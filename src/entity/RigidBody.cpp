#include "RigidBody.h"
#include "../terrain/TerrainFacade.h"
#include "../core/Types.h"

static constexpr float MAX_FALL_SPEED = 1400.0f;

// Checks a single cell (by cell coords) for solidity, bounds-safe.
static bool cell_solid(const TerrainFacade& t, int cx, int cy) {
    if (cx < 0 || cy < 0 || cx >= t.cells_w() || cy >= t.cells_h()) return false;
    MaterialID m = t.get_material(cx, cy);
    return m == MaterialID::DIRT || m == MaterialID::ROCK || m == MaterialID::GOLD_ORE;
}

// ── public ─────────────────────────────────────────────────────────────────

void RigidBody::update(float dt, const TerrainFacade& terrain) {
    // Gravity
    velocity.y += GRAVITY * gravity_scale * dt;
    if (velocity.y > MAX_FALL_SPEED) velocity.y = MAX_FALL_SPEED;

    // X: move then push out
    position.x += velocity.x * dt;
    resolve_x(terrain);

    // Y: move then push out (also sets on_ground)
    on_ground = false;
    position.y += velocity.y * dt;
    resolve_y(terrain);
}

// ── private ────────────────────────────────────────────────────────────────

void RigidBody::resolve_x(const TerrainFacade& terrain) {
    if (velocity.x == 0.0f) return;

    // Shrink the Y range slightly so the feet/head don't catch on single cells
    // when walking horizontally over a 1-cell step.
    float top    = position.y + 2.0f;
    float bottom = position.y + size.y - 2.0f;

    int cy0 = (int)top    / CELL_SIZE;
    int cy1 = (int)bottom / CELL_SIZE;

    if (velocity.x > 0.0f) {
        // Moving right — check right column
        int cx = (int)(position.x + size.x - 0.01f) / CELL_SIZE;
        for (int cy = cy0; cy <= cy1; cy++) {
            if (!cell_solid(terrain, cx, cy)) continue;
            float wall = (float)(cx * CELL_SIZE);
            float pen  = position.x + size.x - wall;
            if (pen > 0.0f) {
                position.x -= pen;
                velocity.x  = 0.0f;
                return;
            }
        }
    } else {
        // Moving left — check left column
        int cx = (int)(position.x) / CELL_SIZE;
        for (int cy = cy0; cy <= cy1; cy++) {
            if (!cell_solid(terrain, cx, cy)) continue;
            float wall = (float)((cx + 1) * CELL_SIZE);
            float pen  = wall - position.x;
            if (pen > 0.0f) {
                position.x += pen;
                velocity.x  = 0.0f;
                return;
            }
        }
    }

    // Map boundary clamp
    if (position.x < 0.0f) {
        position.x = 0.0f; velocity.x = 0.0f;
    }
    float max_x = (float)(terrain.cells_w() * CELL_SIZE) - size.x;
    if (position.x > max_x) {
        position.x = max_x; velocity.x = 0.0f;
    }
}

void RigidBody::resolve_y(const TerrainFacade& terrain) {
    // Shrink the X range slightly to prevent snagging on vertical seams
    float left  = position.x + 1.0f;
    float right = position.x + size.x - 1.0f;

    int cx0 = (int)left  / CELL_SIZE;
    int cx1 = (int)right / CELL_SIZE;

    if (velocity.y > 0.0f) {
        // Falling — check bottom row
        int cy = (int)(position.y + size.y - 0.01f) / CELL_SIZE;
        for (int cx = cx0; cx <= cx1; cx++) {
            if (!cell_solid(terrain, cx, cy)) continue;
            float floor = (float)(cy * CELL_SIZE);
            float pen   = position.y + size.y - floor;
            if (pen > 0.0f) {
                position.y -= pen;
                velocity.y  = 0.0f;
                on_ground   = true;
                return;
            }
        }
    } else if (velocity.y < 0.0f) {
        // Rising — check top row
        int cy = (int)(position.y) / CELL_SIZE;
        for (int cx = cx0; cx <= cx1; cx++) {
            if (!cell_solid(terrain, cx, cy)) continue;
            float ceil_ = (float)((cy + 1) * CELL_SIZE);
            float pen   = ceil_ - position.y;
            if (pen > 0.0f) {
                position.y += pen;
                velocity.y  = 0.0f;
                return;
            }
        }
    }

    // Map boundary clamp
    if (position.y < 0.0f) {
        position.y = 0.0f; velocity.y = 0.0f;
    }
    float max_y = (float)(terrain.cells_h() * CELL_SIZE) - size.y;
    if (position.y > max_y) {
        position.y = max_y; velocity.y = 0.0f; on_ground = true;
    }
}
