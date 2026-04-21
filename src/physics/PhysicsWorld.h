#pragma once

#include "core/Types.h"
#include <box2d/box2d.h>
#include <functional>
#include <memory>

// PhysicsWorld — thin wrapper around b2World.
// Uses pixels as the unit system (no scale conversion — Box2D is used for
// fragment/dynamic-solid physics where pixel accuracy is important).
// Fixed timestep: caller must pass a consistent dt (e.g. 1/60 s).
class PhysicsWorld {
public:
    // World gravity in pixels/s² (positive = downward).
    static constexpr float GRAVITY_Y = 980.0f;

    static constexpr int VELOCITY_ITER = 8;
    static constexpr int POSITION_ITER = 3;

    // Collision callback: (entity_id, material_id_at_contact, impact_speed_px_s)
    using CollisionCallback = std::function<void(EntityID, int, float)>;

    PhysicsWorld();
    ~PhysicsWorld();

    // Advance the simulation by dt seconds.
    void Step(float dt);

    // Factory helpers — callers own the returned b2Body* (destroyed via DestroyBody).
    b2Body* CreateBody(const b2BodyDef& def);
    void    DestroyBody(b2Body* body);

    b2World& GetB2World() { return world_; }

    // Install a collision callback invoked from the internal b2ContactListener.
    void SetCollisionCallback(CollisionCallback cb);

private:
    class ContactRelay;  // internal b2ContactListener subclass

    b2World world_;
    std::unique_ptr<ContactRelay> contact_relay_;
};
