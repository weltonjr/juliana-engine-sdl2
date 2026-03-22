# Aeterium — Architecture Specification

## Stack

| Layer | Technology |
|---|---|
| Language | C++17 |
| Window / Input / Audio | SDL2 |
| Text rendering | SDL2_ttf |
| Build system | CMake 3.14+ (find_package, system raylib via Homebrew) |
| Platform target (v0.1) | macOS (Apple Silicon + Intel) |

---

## Guiding Principles

1. **Terrain bitmap is the source of truth.** Visual, collision, and simulation data are
   derived views — never stored separately as "ground truth."
2. **TerrainFacade is the only public interface.** No system outside `terrain/` touches
   `TerrainBitmap` or `TerrainChunk` directly.
3. **Dirty flags gate all rebuilds.** Chunks only rebuild their GPU texture or collision
   cache when marked dirty. This bounds update cost to what actually changed.
4. **Decoupled resolutions.** Terrain: 4px/cell · Fluid (future): 8px/cell · Fields (future): 16px/cell.
   Each system only reads the coarser systems for boundaries.
5. **All code is additive.** Nothing built in v0.1 is thrown away. v0.2 extends, never replaces.
6. **Flat composition over deep inheritance.** Entities own components (RigidBody, FSM)
   as members. No ECS in v0.1, but data is already structured to migrate to one.

---

## Module Map

```
main.cpp
 └── Game
      ├── TerrainFacade ◀─── only public terrain interface
      │    ├── TerrainBitmap   (private: raw uint8_t array)
      │    └── TerrainChunk[]  (private: dirty flags + GPU texture)
      ├── TerrainRenderer
      │    └── reads TerrainFacade, bakes/draws chunk textures
      ├── TerrainGen          (populates TerrainFacade at startup)
      ├── Character
      │    ├── RigidBody       (position, velocity, AABB terrain collision)
      │    └── CharacterFSM   (state machine: IDLE/WALK/JUMP/FALL/DIG)
      ├── GameCamera          (smooth follow, world bounds clamp)
      ├── InputManager        (SDL2 keyboard → Action enum)
      └── DebugOverlay        (F1 overlay: chunk grid, physics bounds, FSM)
```

---

## Folder Structure

```
aeterium-cpp/
  CMakeLists.txt
  .gitignore
  spec/                         ← design documents (this folder)
  src/
    main.cpp                    ← SDL2 init, game loop, shutdown
    Game.h / Game.cpp           ← owns all systems; update/draw orchestration
    core/
      Types.h                   ← MaterialID enum, physics/cell constants
      Math.h                    ← Vector2, AABB, cell↔world conversions
      Color.h                   ← Color struct + named constants (replaces raylib Color)
    terrain/
      TerrainBitmap.h / .cpp    ← raw 2D array of uint8_t; private to terrain/
      TerrainChunk.h            ← chunk metadata: dirty flags + SDL_Texture* cache
      TerrainFacade.h / .cpp    ← public API: is_solid, get/set_material, dig, damage
      TerrainRenderer.h / .cpp  ← bake dirty chunks → GPU; draw visible chunks
      TerrainGen.h / .cpp       ← procedural map generation (noise-based)
      MaterialColors.h          ← cell color lookup; shared with OreFragment (v0.2)
    entity/
      RigidBody.h / .cpp        ← integrate gravity+velocity; AABB→terrain collision; slope climb
      Character.h / .cpp        ← composes RigidBody + FSM; input-driven controller
      CharacterFSM.h            ← state enum + transition logic
    camera/
      GameCamera.h / .cpp       ← exponential-lerp follow; world bounds clamping
    input/
      InputManager.h / .cpp     ← SDL_GetKeyboardState → Action; rising-edge detection
    debug/
      DebugOverlay.h / .cpp     ← F1 debug HUD (chunk grid, bounds, state, FPS)
```

---

## Data Flow per Frame

```
SDL_PollEvent (InputManager::poll)
      │
      ▼
[Physics tick × N]  (fixed PHYSICS_DT = 1/60s)
  InputManager → Character::update
  Character → TerrainFacade (dig calls → set_material → mark chunk dirty)
  Character → RigidBody::update
    → integrate velocity (gravity applied)
    → resolve_x: AABB push-out + slope step-up
    → resolve_y: AABB push-out + on_ground flag
  GameCamera::follow (lerp toward character center)
      │
      ▼
TerrainRenderer::bake_dirty_chunks  (CPU surface → SDL_Texture for dirty chunks)
      │
      ▼
[Render]
  SDL_RenderClear
  Sky background (flat color gradient)
  TerrainRenderer::draw (blit visible chunk textures)
  Character::draw (colored rectangle + FSM-based debug color)
  DebugOverlay::draw (if F1)
  SDL_RenderPresent
```

---

## Terrain System Detail

### TerrainBitmap

```
std::vector<uint8_t>  data;   // row-major: data[y * width + x]
int width, height;            // in cells
```

- 1 byte per cell = `MaterialID` (0–255, currently 6 used)
- For 512×256 cells: 131,072 bytes ≈ 128 KB (trivially small)
- No compression needed at this scale

### TerrainChunk

```
int chunk_x, chunk_y          // position in chunk grid
bool dirty_visual             // GPU texture needs rebuild
bool dirty_collision          // collision cache needs rebuild (future)
SDL_Texture* tex              // GPU texture: CHUNK_CELLS × CHUNK_CELLS texels
bool tex_valid                // false until first bake
```

Chunk grid for the test map (512×256 cells, chunk=64):
```
CHUNKS_X = 512 / 64 = 8
CHUNKS_Y = 256 / 64 = 4
Total    = 32 chunks
```

### TerrainFacade — Public API

```cpp
// Queries
bool    is_solid(float world_x, float world_y);
uint8_t get_material(int cell_x, int cell_y);
int     cells_w() const;
int     cells_h() const;
int     chunks_x() const;
int     chunks_y() const;

// Mutations (always mark affected chunks dirty)
void  set_material(int cell_x, int cell_y, uint8_t mat);
bool  dig(float world_x, float world_y, float radius);
void  damage(float wx, float wy, float radius);
void  explode(float wx, float wy, float radius, std::vector<OreFragment>& out); // v0.2

// Chunk access (used by TerrainRenderer only)
TerrainChunk& get_chunk(int cx, int cy);
const TerrainChunk& get_chunk(int cx, int cy) const;
```

### Chunk Rendering — GPU Texture Pipeline

```
Mutation (dig/explode)
  └── set_material(cx, cy, EMPTY)
        └── mark_chunk_dirty(cx/64, cy/64)  ← dirty_visual = true

bake_dirty_chunks() [once per update tick]
  └── for each dirty chunk:
        1. SDL_CreateRGBSurfaceWithFormat(CHUNK_CELLS, CHUNK_CELLS, RGBA32)
        2. lock surface; write pixel per cell (material_cell_color)
        3. SDL_CreateTextureFromSurface → upload to GPU
        4. SDL_SetTextureScaleMode(SDL_ScaleModeNearest)  ← sharp pixels
        5. SDL_FreeSurface, dirty_visual = false

draw() [render step]
  └── for each visible chunk with tex_valid:
        SDL_Rect  src = {0, 0, CHUNK_CELLS, CHUNK_CELLS}   (64×64 texels)
        SDL_FRect dst = {sx, sy, CHUNK_PX, CHUNK_PX}       (256×256 screen px)
        SDL_RenderCopyF(renderer, chunk.tex, &src, &dst)   (4× scale, sharp)
```

---

## Entity System

### RigidBody (v0.1)

Composed into every dynamic entity. Not a base class.

```cpp
struct RigidBody {
    Vector2 position;      // top-left corner, world pixels
    Vector2 velocity;      // pixels/second
    Vector2 size;          // AABB width × height
    float   gravity_scale; // 1.0 = normal, 0 = no gravity
    bool    on_ground;     // set by resolve_y each tick

    void update(float dt, const TerrainFacade& terrain);
private:
    void resolve_x(const TerrainFacade&);  // push-out + slope step-up
    void resolve_y(const TerrainFacade&);  // push-out + on_ground
};
```

Collision resolution order: **X first, then Y.** This prevents the character from being
deflected diagonally when hitting corners.

Slope step-up (in resolve_x): when blocked in X and `on_ground` (cell below is solid),
attempt to lift position.y by 1px at a time (up to `STEP_MAX = 8px = 2 cells`). If the
body clears the obstacle at the lifted position, the X movement is allowed and the new Y
is kept. This makes walking up gentle slopes and 1–2 cell ledges seamless.

### Character

```cpp
class Character {
    RigidBody    body;
    CharacterFSM fsm;
    int          coyote_frames;  // countdown after leaving ground
    int          jump_buffer;    // countdown after jump input before landing
    bool         is_digging;
public:
    void update(float dt, const InputManager&, TerrainFacade&);
    void draw(Vector2 camera_offset, SDL_Renderer*) const;
    Vector2 center() const;
    CharacterState state() const;
};
```

### Future Entities (v0.2+)

All will compose `RigidBody` the same way:

| Entity | Type | Notes |
|---|---|---|
| OreFragment | dynamic RigidBody | sprite from bitmap pixels; mass ∝ area |
| Building | kinematic (no gravity) | fixed in world; has slots + HP |
| Projectile | dynamic, CCD | fast-moving; continuous collision |
| Enemy | kinematic + AI FSM | hostile creature |

---

## Input System

```cpp
enum class Action { MOVE_LEFT, MOVE_RIGHT, JUMP, DIG };

class InputManager {
    const Uint8* m_keys;          // SDL_GetKeyboardState snapshot
    bool m_prev[SDL_NUM_SCANCODES]; // previous frame snapshot
    bool m_quit;

    void poll();                   // call once per SDL event loop
    bool is_action_held(Action);   // true while key held
    bool is_action_pressed(Action); // true only on first frame (rising edge)
    bool quit_requested() const;
};
```

Key bindings:

| Action | Primary | Alternate |
|---|---|---|
| MOVE_LEFT | A | ← |
| MOVE_RIGHT | D | → |
| JUMP | Space | ↑ |
| DIG | C | — |
| Debug overlay | F1 | — |
| Free camera | F2 | — |
| Quit | Escape | Window ✕ |

---

## Camera System

```cpp
class GameCamera {
    float m_x, m_y;          // current camera world position (top-left)
    float m_speed = 8.0f;    // lerp speed factor
    int m_screen_w, m_screen_h;
    int m_world_w, m_world_h;

    void follow(Vector2 target, float dt);  // exponential lerp toward target
    void snap_to(Vector2 target);           // instant, no lerp
    void clamp();                           // prevent showing beyond map edges
    Vector2 offset() const;                 // subtract from world pos to get screen pos
};
```

Exponential lerp formula: `t = 1 - pow(0.01, dt * speed)` → camera closes
1% of remaining distance per second at low speed, reacts snappily but never overshoots.

---

## Post-MVP Systems (planned)

### Fluid Simulation (v0.2)
- Cellular automaton on an 8px/cell grid (independent of terrain 4px grid)
- Each cell: `float level` (0.0–1.0 water fill)
- Rules per tick: fall → equalize laterally → rise from pressure
- Reads terrain for solid boundaries (a fluid cell is blocked if any 4px terrain cell covering it is solid)

### Field System (v0.2)
- Generic diffusion grid at 16px/cell (one solver, N registered fields)
- Update: `F[x,y] += dt * rate * (neighbors - 4*F[x,y])` + decay
- Used for: mana, heat, radiation, magic auras
- Mods can register new field types

### Portal System (v0.3)
- Connects two points in the same or different maps
- Physics: "ghost zones" — copy border cells before fluid tick
- Entities: teleport + velocity vector rotation when crossing portal plane
- Rendering: second virtual camera into a framebuffer, blitted with stencil clip

### Multiplayer (TBD)
- Lockstep or client-server (to be decided)
- All game state is deterministic (fixed physics timestep, no floating rand)

---

## Build & Run

```bash
# Prerequisites (macOS with Homebrew)
brew install cmake sdl2 sdl2_ttf

# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --parallel

# Run
./build/aeterium
```
