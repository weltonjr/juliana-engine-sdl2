#pragma once
#include <cstdint>

enum class MaterialID : uint8_t {
    EMPTY    = 0,
    DIRT     = 1,
    ROCK     = 2,
    GOLD_ORE = 3,
    WATER    = 4,
    AIR      = 5,
};

// Terrain resolution: 4 pixels per cell
static constexpr int CELL_SIZE     = 4;
// Chunk size in cells
static constexpr int CHUNK_CELLS   = 64;
// Chunk size in pixels
static constexpr int CHUNK_PX      = CHUNK_CELLS * CELL_SIZE; // 256

// Test map dimensions (in cells)
static constexpr int MAP_CELLS_W   = 256;
static constexpr int MAP_CELLS_H   = 192;

// Map dimensions in pixels
static constexpr int MAP_PX_W      = MAP_CELLS_W * CELL_SIZE; // 1024
static constexpr int MAP_PX_H      = MAP_CELLS_H * CELL_SIZE; // 768

// Chunk grid dimensions
static constexpr int CHUNKS_X      = MAP_CELLS_W / CHUNK_CELLS; // 4
static constexpr int CHUNKS_Y      = MAP_CELLS_H / CHUNK_CELLS; // 3

// Physics
static constexpr float GRAVITY     = 900.0f; // pixels/s^2
static constexpr float PHYSICS_DT  = 1.0f / 60.0f;
