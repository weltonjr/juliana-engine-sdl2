#pragma once

#include "core/FixedPoint.h"

class EntityManager;
class Terrain;
class DefinitionRegistry;
struct Entity;

class PhysicsSystem {
public:
    PhysicsSystem(const DefinitionRegistry& registry);

    void Update(EntityManager& entities, const Terrain& terrain, Fixed dt);

private:
    void ApplyGravity(Entity& entity, Fixed dt);
    void MoveEntity(Entity& entity, const Terrain& terrain, Fixed dt);
    bool CheckTerrainOverlap(const Terrain& terrain, int x, int y, int w, int h) const;
    int TryStepUp(const Terrain& terrain, int x, int y, int w, int h, int max_step) const;

    const DefinitionRegistry& registry_;
    Fixed gravity_;
};
