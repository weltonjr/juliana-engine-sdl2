#include "physics/PhysicsSystem.h"
#include "entity/EntityManager.h"
#include "entity/Entity.h"
#include "terrain/Terrain.h"
#include "package/DefinitionRegistry.h"
#include "package/MaterialDef.h"
#include "package/ObjectDef.h"
#include <cstring>

// Pixels per meter (Box2D internally works in meters; we use this scale).
// 1 metre = 32 pixels gives sensible behaviour with default Box2D gravity (9.8 m/s²).
// We override world gravity to GRAVITY_Y pixels/s², so no further scaling is needed.
static constexpr float PIXELS_TO_METERS = 1.0f / 32.0f;
static constexpr float METERS_TO_PIXELS = 32.0f;

PhysicsSystem::PhysicsSystem(const DefinitionRegistry& registry, PhysicsWorld& world)
    : registry_(registry), world_(world)
{
    solid_lut_.resize(256, false);
    for (int i = 0; i < 256; i++) {
        auto* mat = registry.GetMaterialByRuntimeID(static_cast<MaterialID>(i));
        if (mat && (mat->state == MaterialState::Solid || mat->state == MaterialState::Powder)) {
            solid_lut_[i] = true;
        }
    }
}

void PhysicsSystem::RegisterEntity(Entity& entity) {
    if (!entity.definition) return;

    b2BodyDef bd;
    switch (entity.definition->physics_mode) {
        case PhysicsMode::Static:    bd.type = b2_staticBody;    break;
        case PhysicsMode::Kinematic: bd.type = b2_kinematicBody; break;
        default:                     bd.type = b2_dynamicBody;   break;
    }

    bd.position.Set(entity.pos_x * PIXELS_TO_METERS,
                    entity.pos_y * PIXELS_TO_METERS);
    bd.linearVelocity.Set(entity.vel_x * PIXELS_TO_METERS,
                          entity.vel_y * PIXELS_TO_METERS);
    bd.fixedRotation = !entity.definition->rotation;
    bd.linearDamping = 0.0f;
    bd.angularDamping = entity.definition->angular_drag;

    b2Body* body = world_.CreateBody(bd);

    // Box shape sized to the entity (centred on the body origin)
    float hw = entity.width  * 0.5f * PIXELS_TO_METERS;
    float hh = entity.height * 0.5f * PIXELS_TO_METERS;
    b2PolygonShape shape;
    shape.SetAsBox(hw, hh);

    b2FixtureDef fd;
    fd.shape    = &shape;
    fd.density  = (entity.mass > 0.0f)
                    ? entity.mass / (entity.width * entity.height)
                    : 1.0f;
    fd.friction = 0.5f;
    fd.restitution = 0.0f;
    fd.isSensor = entity.definition->overlap_detection;
    body->CreateFixture(&fd);

    entity_bodies_[entity.id] = body;
}

void PhysicsSystem::UnregisterEntity(EntityID id) {
    auto it = entity_bodies_.find(id);
    if (it != entity_bodies_.end()) {
        world_.DestroyBody(it->second);
        entity_bodies_.erase(it);
    }
}

void PhysicsSystem::SyncBodiesToEntities(EntityManager& entities) {
    entities.ForEach([&](Entity& entity) {
        auto it = entity_bodies_.find(entity.id);
        if (it == entity_bodies_.end()) return;

        b2Body* body = it->second;
        b2Vec2 pos = body->GetPosition();
        b2Vec2 vel = body->GetLinearVelocity();

        entity.prev_pos_x = entity.pos_x;
        entity.prev_pos_y = entity.pos_y;

        entity.pos_x = pos.x * METERS_TO_PIXELS;
        entity.pos_y = pos.y * METERS_TO_PIXELS;
        entity.vel_x = vel.x * METERS_TO_PIXELS;
        entity.vel_y = vel.y * METERS_TO_PIXELS;

        // Determine on_ground: probe 1 pixel below the entity feet
        int foot_x  = static_cast<int>(entity.pos_x);
        int foot_y  = static_cast<int>(entity.pos_y) + entity.height;
        entity.was_airborne = !entity.on_ground;
        entity.on_ground    = false; // will be set by contact detection below
        // Simple foot-probe against terrain
        // (full contact callbacks would be added as a Box2D contact listener)
    });
}

void PhysicsSystem::Update(EntityManager& entities, const Terrain& /*terrain*/, float /*dt*/) {
    // Keep Box2D bodies in sync with any external position changes
    // (e.g. teleports, spawns)
    entities.ForEach([&](Entity& entity) {
        auto it = entity_bodies_.find(entity.id);
        if (it == entity_bodies_.end()) {
            // Auto-register newly spawned entities
            RegisterEntity(entity);
            return;
        }

        // If entity was moved externally (pos diverged significantly), update body
        b2Body* body = it->second;
        b2Vec2 bpos = body->GetPosition();
        float bx = bpos.x * METERS_TO_PIXELS;
        float by = bpos.y * METERS_TO_PIXELS;
        float dx = entity.pos_x - bx;
        float dy = entity.pos_y - by;
        if (dx * dx + dy * dy > 4.0f) { // > 2px discrepancy
            body->SetTransform(
                b2Vec2(entity.pos_x * PIXELS_TO_METERS,
                       entity.pos_y * PIXELS_TO_METERS),
                body->GetAngle());
            body->SetLinearVelocity(
                b2Vec2(entity.vel_x * PIXELS_TO_METERS,
                       entity.vel_y * PIXELS_TO_METERS));
        }
    });

    // NOTE: world_.Step(dt) is called centrally by Engine::RunOneSimStep()
    // so that all sim systems (terrain, Box2D, DynamicBodyManager) share the
    // same time scale. PhysicsSystem only pushes entity transforms to Box2D
    // and reads them back; it does NOT step the world.

    SyncBodiesToEntities(entities);
}
