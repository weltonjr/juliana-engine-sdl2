#pragma once
#include "RigidBody.h"
#include "CharacterFSM.h"
#include "raylib.h"

class TerrainFacade;
class InputManager;

class Character {
public:
    Character();

    void update(float dt, const InputManager& input, TerrainFacade& terrain);
    void draw(Vector2 cam_offset) const;

    // Read-only accessors for other systems (e.g. camera)
    Vector2    position() const { return m_body.position; }
    Vector2    center()   const;
    CharState  state()    const { return m_state; }

private:
    void apply_input  (const InputManager& input);
    void update_state ();

    RigidBody m_body;
    CharState m_state     = CharState::FALL;
    int       m_facing    = 1;   // +1 = right, -1 = left

    // Coyote time: can still jump briefly after leaving a ledge
    float m_coyote_timer      = 0.0f;
    static constexpr float COYOTE_TIME = 0.10f;   // seconds

    // Jump buffer: jump pressed slightly before landing still triggers
    float m_jump_buffer_timer = 0.0f;
    static constexpr float JUMP_BUFFER  = 0.10f;  // seconds

    // Character tuning
    static constexpr float WALK_SPEED  = 160.0f;  // px/s
    static constexpr float JUMP_VEL    = -440.0f; // px/s (negative = up)
    static constexpr float DIG_RADIUS  = 16.0f;   // px
};
