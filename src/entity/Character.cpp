#include "Character.h"
#include "../input/InputManager.h"
#include "../terrain/TerrainFacade.h"
#include "../core/Types.h"

// ── spawn position: center of map, just above the surface (~15% from top) ──
static constexpr float SPAWN_X = MAP_PX_W * 0.5f - 6.0f;
static constexpr float SPAWN_Y = MAP_PX_H * 0.18f;  // above surface (~28% of map height)

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
    float w  = m_body.size.x;   // 12px
    float h  = m_body.size.y;   // 20px

    // ── Body (lower 12px) ──
    // Color shifts slightly by state as a subtle feedback cue
    Color suit_color;
    switch (m_state) {
        case CharState::IDLE: suit_color = {60,  90,  180, 255}; break;
        case CharState::WALK: suit_color = {50,  150, 80,  255}; break;
        case CharState::JUMP: suit_color = {180, 160, 40,  255}; break;
        case CharState::FALL: suit_color = {190, 100, 30,  255}; break;
        case CharState::DIG:  suit_color = {180, 50,  50,  255}; break;
        default:              suit_color = {80,  80,  80,  255}; break;
    }
    DrawRectangle((int)sx,       (int)(sy + 10), (int)w, 10, suit_color);

    // ── Legs (two small rectangles, animate when walking) ──
    bool step = (m_state == CharState::WALK);
    int leg_offset = step ? (int)(GetTime() * 8) % 2 : 0; // alternates 0/1
    DrawRectangle((int)sx + 1, (int)(sy + 16) + leg_offset,     4, 4, suit_color);
    DrawRectangle((int)sx + 7, (int)(sy + 16) + (1 - leg_offset), 4, 4, suit_color);

    // ── Head (circle, skin tone) ──
    Color skin = {220, 170, 120, 255};
    DrawCircle((int)(sx + w * 0.5f), (int)(sy + 6), 6, skin);

    // ── Helmet ──
    Color helmet = suit_color;
    helmet.r = (unsigned char)(helmet.r > 50 ? helmet.r - 30 : 0);
    DrawRectangle((int)(sx + 1), (int)(sy - 1), (int)w - 2, 5, helmet);

    // ── Eyes (direction-aware) ──
    // Eye white
    int eye_x = (m_facing > 0) ? (int)(sx + 7) : (int)(sx + 2);
    DrawRectangle(eye_x, (int)(sy + 4), 3, 3, WHITE);
    // Pupil: shifts one pixel in facing direction
    int pupil_x = (m_facing > 0) ? eye_x + 1 : eye_x;
    DrawRectangle(pupil_x, (int)(sy + 5), 2, 2, {30, 30, 30, 255});
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
