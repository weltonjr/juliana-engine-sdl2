#include "Character.h"
#include "../input/InputManager.h"
#include "../terrain/TerrainFacade.h"
#include "../core/Types.h"

// ── spawn position: center of map, just above the surface (~15% from top) ──
static constexpr float SPAWN_X = MAP_PX_W * 0.5f - 6.0f;
static constexpr float SPAWN_Y = MAP_PX_H * 0.13f;

Character::Character() {
    m_body.position = {SPAWN_X, SPAWN_Y};
    m_body.size     = {12.0f, 20.0f};
}

// ── public ─────────────────────────────────────────────────────────────────

void Character::update(float dt, const InputManager& input, TerrainFacade& terrain) {
    // Tick timers
    if (m_coyote_timer      > 0.0f) m_coyote_timer      -= dt;
    if (m_jump_buffer_timer > 0.0f) m_jump_buffer_timer -= dt;

    // Record whether we were on the ground before physics
    bool was_on_ground = m_body.on_ground;

    apply_input(input);

    m_body.update(dt, terrain);

    // Coyote time: starts counting when we leave the ground
    if (was_on_ground && !m_body.on_ground) {
        m_coyote_timer = COYOTE_TIME;
    }

    // Jump buffer: if we just landed and there's a buffered jump, fire it
    if (!was_on_ground && m_body.on_ground && m_jump_buffer_timer > 0.0f) {
        m_body.velocity.y  = JUMP_VEL;
        m_body.on_ground   = false;
        m_jump_buffer_timer = 0.0f;
    }

    // Dig: create a hole in the terrain in front of the character
    if (input.is_pressed(Action::DIG)) {
        float dig_x = m_body.position.x + m_body.size.x * 0.5f + m_facing * DIG_RADIUS;
        float dig_y = m_body.position.y + m_body.size.y * 0.5f;
        terrain.dig(dig_x, dig_y, DIG_RADIUS);
    }

    update_state();
}

void Character::draw(Vector2 cam_offset) const {
    float sx = m_body.position.x - cam_offset.x;
    float sy = m_body.position.y - cam_offset.y;
    float w  = m_body.size.x;
    float h  = m_body.size.y;

    // Body color by state (useful as debug indicator)
    Color body_color;
    switch (m_state) {
        case CharState::IDLE: body_color = BLUE;   break;
        case CharState::WALK: body_color = GREEN;  break;
        case CharState::JUMP: body_color = YELLOW; break;
        case CharState::FALL: body_color = ORANGE; break;
        case CharState::DIG:  body_color = RED;    break;
        default:              body_color = WHITE;  break;
    }

    DrawRectangle((int)sx, (int)sy, (int)w, (int)h, body_color);

    // Facing indicator: small dot on the front side
    float dot_x = (m_facing > 0) ? sx + w - 3 : sx + 1;
    float dot_y = sy + 4;
    DrawRectangle((int)dot_x, (int)dot_y, 2, 2, WHITE);
}

Vector2 Character::center() const {
    return {
        m_body.position.x + m_body.size.x * 0.5f,
        m_body.position.y + m_body.size.y * 0.5f
    };
}

// ── private ────────────────────────────────────────────────────────────────

void Character::apply_input(const InputManager& input) {
    // Horizontal movement
    m_body.velocity.x = 0.0f;
    if (input.is_held(Action::MOVE_LEFT)) {
        m_body.velocity.x = -WALK_SPEED;
        m_facing = -1;
    }
    if (input.is_held(Action::MOVE_RIGHT)) {
        m_body.velocity.x =  WALK_SPEED;
        m_facing =  1;
    }

    // Jump: allowed from ground or within coyote window
    if (input.is_pressed(Action::JUMP)) {
        if (m_body.on_ground || m_coyote_timer > 0.0f) {
            m_body.velocity.y  = JUMP_VEL;
            m_body.on_ground   = false;
            m_coyote_timer     = 0.0f;
        } else {
            // Buffer the jump — will fire on next landing
            m_jump_buffer_timer = JUMP_BUFFER;
        }
    }
}

void Character::update_state() {
    if (!m_body.on_ground) {
        m_state = (m_body.velocity.y < 0.0f) ? CharState::JUMP : CharState::FALL;
        return;
    }
    if (m_body.velocity.x != 0.0f) {
        m_state = CharState::WALK;
    } else {
        m_state = CharState::IDLE;
    }
}
