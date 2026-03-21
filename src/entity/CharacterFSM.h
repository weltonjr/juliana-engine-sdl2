#pragma once

enum class CharState {
    IDLE,
    WALK,
    JUMP,   // rising (velocity.y < 0)
    FALL,   // falling (velocity.y >= 0, not on ground)
    DIG,    // actively digging
};

// Returns a debug-friendly name
inline const char* char_state_name(CharState s) {
    switch (s) {
        case CharState::IDLE: return "IDLE";
        case CharState::WALK: return "WALK";
        case CharState::JUMP: return "JUMP";
        case CharState::FALL: return "FALL";
        case CharState::DIG:  return "DIG";
    }
    return "?";
}
