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
