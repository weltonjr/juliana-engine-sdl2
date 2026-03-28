#include "physics/PhysicsSystem.h"
#include "entity/EntityManager.h"
#include "entity/Entity.h"
#include "terrain/Terrain.h"
#include "package/DefinitionRegistry.h"
#include "package/MaterialDef.h"
#include "package/ObjectDef.h"

PhysicsSystem::PhysicsSystem(const DefinitionRegistry& registry)
    : registry_(registry)
    , gravity_(800.0f)
{
    // Build fast solid lookup table
    solid_lut_.resize(256, false);
    for (int i = 0; i < 256; i++) {
        auto* mat = registry.GetMaterialByRuntimeID(static_cast<MaterialID>(i));
        if (mat && (mat->state == MaterialState::Solid || mat->state == MaterialState::Powder)) {
            solid_lut_[i] = true;
        }
    }
}

bool PhysicsSystem::CheckTerrainOverlap(const Terrain& terrain, int x, int y, int w, int h) const {
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            Cell cell = terrain.GetCell(px, py);
            if (solid_lut_[cell.material_id]) {
                return true;
            }
        }
    }
    return false;
}

int PhysicsSystem::TryStepUp(const Terrain& terrain, int x, int y, int w, int h, int max_step) const {
    for (int step = 1; step <= max_step; step++) {
        if (!CheckTerrainOverlap(terrain, x, y - step, w, h)) {
            return step;
        }
    }
    return 0;
}

void PhysicsSystem::ApplyGravity(Entity& entity, float dt) {
    if (entity.definition && entity.definition->physics_mode != PhysicsMode::Dynamic) return;

    if (entity.on_ground) {
        if (entity.vel_y > 0.0f) {
            entity.vel_y = 0.0f;
        }
        return;
    }

    entity.vel_y += gravity_ * dt;

    float max_fall = entity.max_fall_speed;
    if (entity.vel_y > max_fall) {
        entity.vel_y = max_fall;
    }
}

void PhysicsSystem::MoveEntity(Entity& entity, const Terrain& terrain, float dt) {
    if (entity.definition && entity.definition->physics_mode != PhysicsMode::Dynamic) return;

    entity.prev_pos_x = entity.pos_x;
    entity.prev_pos_y = entity.pos_y;
    entity.was_airborne = !entity.on_ground;

    // Move X
    float new_x = entity.pos_x + entity.vel_x * dt;
    int ix = static_cast<int>(new_x);
    int iy = static_cast<int>(entity.pos_y);

    if (CheckTerrainOverlap(terrain, ix, iy, entity.width, entity.height)) {
        int step = TryStepUp(terrain, ix, iy, entity.width, entity.height, entity.step_up);
        if (step > 0) {
            entity.pos_x = new_x;
            entity.pos_y = entity.pos_y - static_cast<float>(step);
        } else {
            entity.vel_x = 0.0f;
        }
    } else {
        entity.pos_x = new_x;
    }

    // Move Y
    float new_y = entity.pos_y + entity.vel_y * dt;
    ix = static_cast<int>(entity.pos_x);
    int new_iy = static_cast<int>(new_y);

    if (CheckTerrainOverlap(terrain, ix, new_iy, entity.width, entity.height)) {
        if (entity.vel_y > 0.0f) {
            entity.on_ground = true;
            int cur_iy = static_cast<int>(entity.pos_y);
            for (int test_y = cur_iy; test_y <= new_iy; test_y++) {
                if (CheckTerrainOverlap(terrain, ix, test_y, entity.width, entity.height)) {
                    entity.pos_y = static_cast<float>(test_y - 1);
                    break;
                }
            }
        } else {
            entity.pos_y = static_cast<float>(new_iy + 1);
        }
        entity.vel_y = 0.0f;
    } else {
        entity.pos_y = new_y;
        entity.on_ground = CheckTerrainOverlap(terrain, ix, new_iy + entity.height, entity.width, 1);
    }
}

void PhysicsSystem::Update(EntityManager& entities, const Terrain& terrain, float dt) {
    entities.ForEach([&](Entity& entity) {
        ApplyGravity(entity, dt);
        MoveEntity(entity, terrain, dt);
    });
}
