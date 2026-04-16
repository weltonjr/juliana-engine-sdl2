#include "physics/PhysicsWorld.h"
#include <cmath>
#include <memory>

// ── ContactRelay — internal b2ContactListener ────────────────────────────────
// Forwards Box2D BeginContact events into the single CollisionCallback sink
// registered via SetCollisionCallback().  Entity bodies store their EntityID in
// b2Body user data; non-entity bodies (DynamicBodyManager fragments) store 0.

class PhysicsWorld::ContactRelay : public b2ContactListener {
public:
    void BeginContact(b2Contact* contact) override {
        if (!callback_) return;

        b2Fixture* fA = contact->GetFixtureA();
        b2Fixture* fB = contact->GetFixtureB();
        if (!fA || !fB) return;

        b2Body* bodyA = fA->GetBody();
        b2Body* bodyB = fB->GetBody();
        if (!bodyA || !bodyB) return;

        // Compute relative impact speed along the contact normal.
        b2Vec2 velA = bodyA->GetLinearVelocity();
        b2Vec2 velB = bodyB->GetLinearVelocity();
        b2WorldManifold wm;
        contact->GetWorldManifold(&wm);
        b2Vec2 rel = velA - velB;
        float impact = std::abs(b2Dot(rel, wm.normal));

        // user data encodes EntityID (0 = not an entity body).
        auto eidA = static_cast<EntityID>(reinterpret_cast<uintptr_t>(bodyA->GetUserData().pointer));
        auto eidB = static_cast<EntityID>(reinterpret_cast<uintptr_t>(bodyB->GetUserData().pointer));

        // Fire callback for each entity involved (material_id=0 placeholder
        // when both sides are entities; terrain contact would need a terrain
        // reference to sample the cell — left as 0 for now).
        if (eidA != 0) callback_(eidA, 0, impact);
        if (eidB != 0) callback_(eidB, 0, impact);
    }

    PhysicsWorld::CollisionCallback callback_;
};

// ── PhysicsWorld implementation ──────────────────────────────────────────────

PhysicsWorld::PhysicsWorld()
    : world_(b2Vec2(0.0f, GRAVITY_Y))
    , contact_relay_(std::make_unique<ContactRelay>())
{
    world_.SetAllowSleeping(true);
    world_.SetContactListener(contact_relay_.get());
}

PhysicsWorld::~PhysicsWorld() = default;

void PhysicsWorld::Step(float dt) {
    world_.Step(dt, VELOCITY_ITER, POSITION_ITER);
}

b2Body* PhysicsWorld::CreateBody(const b2BodyDef& def) {
    return world_.CreateBody(&def);
}

void PhysicsWorld::DestroyBody(b2Body* body) {
    if (body) world_.DestroyBody(body);
}

void PhysicsWorld::SetCollisionCallback(CollisionCallback cb) {
    contact_relay_->callback_ = std::move(cb);
}
