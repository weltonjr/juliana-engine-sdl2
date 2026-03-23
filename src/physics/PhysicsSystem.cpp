#include "physics/PhysicsSystem.h"
#include "entity/EntityManager.h"
#include "terrain/Terrain.h"
#include "package/DefinitionRegistry.h"
#include "package/MaterialDef.h"

PhysicsSystem::PhysicsSystem(const DefinitionRegistry& registry)
    : registry_(registry)
    , gravity_(Fixed::FromInt(800))
{
}

bool PhysicsSystem::CheckTerrainOverlap(const Terrain& terrain, int x, int y, int w, int h) const {
    // Check if the AABB at (x,y) with size (w,h) overlaps any solid/powder terrain
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            Cell cell = terrain.GetCell(px, py);
            auto* mat = registry_.GetMaterialByRuntimeID(cell.material_id);
            if (mat && (mat->state == MaterialState::Solid || mat->state == MaterialState::Powder)) {
                return true;
            }
        }
    }
    return false;
}

int PhysicsSystem::TryStepUp(const Terrain& terrain, int x, int y, int w, int h, int max_step) const {
    // Try raising entity by 1..max_step pixels to clear the obstacle
    for (int step = 1; step <= max_step; step++) {
        if (!CheckTerrainOverlap(terrain, x, y - step, w, h)) {
            return step;
        }
    }
    return 0; // couldn't step up
}

void PhysicsSystem::ApplyGravity(Entity& entity, Fixed dt) {
    if (entity.definition && entity.definition->physics_mode != PhysicsMode::Dynamic) return;

    entity.vel_y += gravity_ * dt;

    // Clamp to max fall speed
    Fixed max_fall = Fixed::FromFloat(entity.max_fall_speed);
    if (entity.vel_y > max_fall) {
        entity.vel_y = max_fall;
    }
}

void PhysicsSystem::MoveEntity(Entity& entity, const Terrain& terrain, Fixed dt) {
    if (entity.definition && entity.definition->physics_mode != PhysicsMode::Dynamic) return;

    // Save previous position for interpolation
    entity.prev_pos_x = entity.pos_x;
    entity.prev_pos_y = entity.pos_y;

    entity.was_airborne = !entity.on_ground;

    // Move X
    Fixed new_x = entity.pos_x + entity.vel_x * dt;
    int ix = new_x.ToInt();
    int iy = entity.pos_y.ToInt();

    if (CheckTerrainOverlap(terrain, ix, iy, entity.width, entity.height)) {
        // Try step-up
        int step = TryStepUp(terrain, ix, iy, entity.width, entity.height, entity.step_up);
        if (step > 0) {
            entity.pos_x = new_x;
            entity.pos_y = entity.pos_y - Fixed::FromInt(step);
        } else {
            // Blocked
            entity.vel_x = Fixed::Zero();
        }
    } else {
        entity.pos_x = new_x;
    }

    // Move Y
    Fixed new_y = entity.pos_y + entity.vel_y * dt;
    ix = entity.pos_x.ToInt();
    int new_iy = new_y.ToInt();

    if (CheckTerrainOverlap(terrain, ix, new_iy, entity.width, entity.height)) {
        if (entity.vel_y > Fixed::Zero()) {
            // Moving down — landed on ground
            entity.on_ground = true;

            // Find exact ground position
            int cur_iy = entity.pos_y.ToInt();
            for (int test_y = cur_iy; test_y <= new_iy; test_y++) {
                if (CheckTerrainOverlap(terrain, ix, test_y, entity.width, entity.height)) {
                    entity.pos_y = Fixed::FromInt(test_y - 1);
                    break;
                }
            }
        } else {
            // Moving up — hit ceiling
            entity.pos_y = Fixed::FromInt(new_iy + 1);
        }
        entity.vel_y = Fixed::Zero();
    } else {
        entity.pos_y = new_y;
        // Check if still on ground (one pixel below)
        entity.on_ground = CheckTerrainOverlap(terrain, ix, new_iy + 1, entity.width, 1);
    }
}

void PhysicsSystem::Update(EntityManager& entities, const Terrain& terrain, Fixed dt) {
    entities.ForEach([&](Entity& entity) {
        ApplyGravity(entity, dt);
        MoveEntity(entity, terrain, dt);
    });
}
