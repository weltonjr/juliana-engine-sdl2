#pragma once

#include "package/MaterialDef.h"
#include <vector>

class EntityManager;
class Terrain;
class DefinitionRegistry;
struct Entity;

class PhysicsSystem {
public:
    PhysicsSystem(const DefinitionRegistry& registry);

    void Update(EntityManager& entities, const Terrain& terrain, float dt);

private:
    void ApplyGravity(Entity& entity, float dt);
    void MoveEntity(Entity& entity, const Terrain& terrain, float dt);
    bool CheckTerrainOverlap(const Terrain& terrain, int x, int y, int w, int h) const;
    int TryStepUp(const Terrain& terrain, int x, int y, int w, int h, int max_step) const;

    const DefinitionRegistry& registry_;
    float gravity_ = 800.0f;

    // Fast collision lookup: true if material blocks movement
    std::vector<bool> solid_lut_;
};
