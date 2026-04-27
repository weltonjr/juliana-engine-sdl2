#pragma once

#include "physics/PhysicsWorld.h"
#include "package/MaterialDef.h"
#include "core/Types.h"
#include <vector>
#include <unordered_map>

class EntityManager;
class Terrain;
class DefinitionRegistry;
struct Entity;

class PhysicsSystem {
public:
    PhysicsSystem(const DefinitionRegistry& registry, PhysicsWorld& world);

    void Update(EntityManager& entities, const Terrain& terrain, float dt);

    // Register a newly spawned entity into Box2D. Call once after entity creation.
    void RegisterEntity(Entity& entity);

    // Remove an entity from Box2D. Call before entity destruction.
    void UnregisterEntity(EntityID id);

    // Direct force/impulse application — bypasses entity struct and writes to Box2D body.
    // Use these from Lua bindings rather than modifying entity.vel_x/y directly.
    void ApplyImpulse(EntityID id, float ix, float iy);
    void ApplyForce(EntityID id, float fx, float fy);
    void ApplyTorque(EntityID id, float torque);
    void SetAngularVelocity(EntityID id, float rad_s);
    void SetVelocity(EntityID id, float vx, float vy);
    void SetPosition(EntityID id, float x, float y);
    float GetAngle(EntityID id) const;

    PhysicsWorld& GetWorld() { return world_; }

private:
    void SyncBodiesToEntities(EntityManager& entities);

    const DefinitionRegistry& registry_;
    PhysicsWorld&             world_;

    // Map entity id → Box2D body
    std::unordered_map<EntityID, b2Body*> entity_bodies_;

    // Fast terrain-solid lookup (indexed by MaterialID)
    std::vector<bool> solid_lut_;
};
