# Networking Architecture

## Overview

Networking is not implemented in v0.1, but the engine is designed from day one to support it. The key constraints — input abstraction, simulation/render separation, deterministic physics, terrain batching — are built into the architecture so that adding networking later doesn't require a rewrite.

## Authority Model

**Server-authoritative**. One machine runs the simulation. Clients send inputs and receive state.

- The **host player** runs the server + their own client (no dedicated server binary needed initially)
- The server ticks the world at a configurable fixed rate (default 60Hz)
- Clients send **input packets** (which actions are pressed, aim direction)
- The server runs all Lua scripts, resolves physics, simulates terrain, and sends state updates back
- **Lua scripts only run on the server**. Clients are renderers + input senders.

This means script authors never think about networking. They always see the authoritative world. The engine handles sync transparently.

## Deterministic Simulation

The simulation is **strictly deterministic**: given the same inputs and initial state, the simulation produces identical results on any machine. This enables:

- **Replay**: record inputs, replay the game perfectly
- **Save-state verification**: compare checksums between server and client
- **Lockstep fallback**: if needed in the future, clients can run the sim locally
- **Client-side prediction**: client predicts local player movement, server corrects if diverged

### Determinism Rules

| Rule | Implementation |
|---|---|
| **Seeded PRNG** | Lua's `math.random` is replaced with a deterministic PRNG seeded from the game seed. Each entity gets its own PRNG stream to avoid order-dependent results. |
| **Fixed tick rate** | Simulation advances in fixed steps. No delta-time variance. `dt` is always `1.0 / tick_rate`. |
| **Deterministic iteration order** | Entity tick order is by entity ID (monotonic). Terrain simulation scans top-to-bottom, left-to-right. |
| **No OS-dependent behavior** | No system clock, no thread-dependent ordering in simulation code. |
| **No float in Lua simulation APIs** | Engine APIs that return positions/velocities to Lua return fixed-point values (presented as integers or scaled integers). |


## Input Abstraction

The engine never reads keyboard/gamepad state directly in gameplay code. All input goes through a player→action mapping:

```
Physical Input → InputSystem (raw) → PlayerInputMap → InputManager → Engine/Entity
```

### Current Implementation

`InputManager` (`src/input/InputManager.h`) owns:
- One `InputSystem` for raw SDL keyboard/mouse state
- One `PlayerInputMap` per player slot (each with its own rebindable `InputBinding` per action)

`InputAction` (`src/input/InputAction.h`) defines game-concept actions:

| Action | Default Key(s) |
|---|---|
| `MoveLeft` | A |
| `MoveRight` | D |
| `Jump` | W or Space |
| `DigDown` | Q + S |
| `DigHorizontal` | C (+ movement direction) |
| `PrevCharacter` | 1 |
| `NextCharacter` | 3 |
| `Quit` | Escape |

`InputBinding` (`src/input/InputBinding.h`) maps an action to `primary`, optional `modifier` (combo), and optional `alt` key. Bindings can be changed at runtime via `InputManager::SetBinding(slot, action, binding)`.

### Future InputSources

| InputSource | Description |
|---|---|
| `local_keyboard` | Current implementation — reads from `InputSystem` (SDL keyboard) |
| `local_gamepad` | Future — reads from a specific SDL gamepad for splitscreen player 2+ |
| `network` | Future — reads from network input packets (remote player) |
| `ai` | Future — reads from AI controller script |

Each future source maps its physical signals onto the same `InputAction` enum. The rest of the engine remains unchanged.

## Entity Ownership

Each entity has an **owner player slot** (or `nil` for unowned entities like wildlife, projectiles, environment objects):

```lua
-- Get which player slot owns this entity
local slot = GetOwner(entity_id)    -- returns slot index or nil

-- Set owner (used during spawn)
SetOwner(entity_id, slot_index)

-- Check if this entity is controlled by the local player (for UI purposes)
local is_local = IsLocalPlayer(slot_index)
```

Ownership determines:
- Which input source the entity reads from
- Which player's camera follows this entity
- Which player's UI shows this entity's inventory
- Network: which client receives detailed updates for this entity

## Simulation / Render Separation

The game loop cleanly separates fixed-rate simulation from variable-rate rendering:

```
Game Loop:
│
├── Simulation Tick (fixed rate, configurable, default 60Hz)
│   ├── Collect inputs (local keyboard/gamepad, network packets, AI)
│   ├── Run entity procedures (walk, flight, swim, etc.)
│   ├── Run Lua scripts (OnTick for all entities and scenario aspects)
│   ├── Resolve physics (collision detection and response)
│   ├── Simulate terrain (powder gravity, liquid flow, pressure equalization)
│   ├── Apply pending terrain changes (batched)
│   ├── Generate terrain diff (for future network sync)
│   ├── Process entity spawns and destroys (queued during tick)
│   └── Advance action animations (frame counting, transitions)
│
├── Render Frame (variable rate, vsync or uncapped)
│   ├── Interpolate entity positions (between last two sim states, using alpha)
│   ├── Draw sky / parallax background
│   ├── Draw background terrain cells
│   ├── Draw foreground terrain cells
│   ├── Draw entities (sprite at interpolated position)
│   ├── Draw particles / effects
│   └── Draw UI / HUD
│
└── Interpolation Alpha:
    alpha = accumulated_time / tick_duration
    render_pos = prev_pos + (curr_pos - prev_pos) * alpha
```

This separation means:
- Server can run simulation without rendering (headless dedicated server in the future)
- Client can render at 144fps while simulation runs at 60Hz
- Network clients interpolate between received states for smooth visuals

## Terrain Change Batching

All terrain modifications within a tick are collected, not applied immediately:

```
During tick:
  DigCircle(x, y, r)      → appends to pending_changes
  BlastCircle(x, y, r, p) → appends to pending_changes
  Liquid simulation        → appends to pending_changes
  Material scripts         → appends to pending_changes

End of tick:
  1. Apply all pending_changes to the terrain grid
  2. Generate diff (list of {x, y, old_material, new_material, old_bg, new_bg})
  3. Store diff for network sync (future)
  4. Trigger visual updates (dirty-rect the affected render chunks)
  5. Flag affected liquid bodies for recalculation next tick
```

Benefits:
- Order-independent: multiple scripts modifying the same cell in one tick resolve predictably (last write wins, deterministic order by entity ID)
- Network-ready: the diff is already computed, just serialize and send
- Efficient rendering: dirty-rect updates instead of full terrain redraw

## Server Configuration

Server settings are configurable via `server.toml` at the game root:

```toml
[server]
tick_rate = 60               # simulation ticks per second (10–120)
max_players = 4              # maximum connected players
port = 7777                  # network port (future)

[simulation]
gravity = 800                # downward acceleration (fixed-point units per tick²)
terrain_sim_budget_ms = 4.0  # max ms per tick for terrain simulation
entity_limit = 2048          # max simultaneous entities
max_step_up = 8              # engine-wide maximum step-up height (pixels)
```

When running single-player or split-screen, the server runs in-process with default settings. The `server.toml` is only needed for customization or dedicated server deployments.

## Client-Side Prediction (Future)

For responsive controls over network latency, the client will predict the local player's movement:

1. Client receives server state at tick N
2. Client applies local inputs for ticks N+1, N+2, ... running the procedure locally
3. When server confirms tick N+1, client compares — if they match, prediction was correct
4. If they diverge, client snaps to server state and re-predicts from there

This requires procedures to be **deterministic and side-effect-free** — they only modify the owning entity's position/velocity based on input and terrain. Our procedure system already supports this since procedures are isolated scripts.

## Network Sync Summary (Future Implementation)

| Data | Sync Method | Frequency |
|---|---|---|
| Entity position/velocity/facing | Delta-compressed state | Every tick (only changed entities) |
| Entity action + frame | Event | On change |
| Terrain changes | Batched diffs | End of tick (only if changes occurred) |
| Entity spawn/destroy | Event | On occurrence |
| Container changes (own inventory) | Event | On change |
| Sound triggers | Event | On trigger (positional, clients spatialize locally) |
| Particle triggers | Event | On trigger (clients spawn locally) |
| Full terrain (join) | Compressed bulk transfer | Once on connect |

## What Must Be Built Now (v0.1)

Even without networking code, these systems are required from the start:

| System | Reason |
|---|---|
| Input abstraction (player → action mapping) | **Implemented** — `InputManager` / `PlayerInputMap` / `InputAction`. Split-screen adds more slots. |
| Fixed-point physics | **Deferred** — `FixedPoint.h` preserved; entity/physics use floats currently. Must be restored before networking. |
| Seeded PRNG per entity | Determinism. Lua `math.random` must be replaced from day one. |
| Fixed tick + render interpolation | Smooth rendering. Headless server capability. |
| Terrain change batching | Clean architecture. Network diff generation comes free. |
| Entity ownership (player slot) | Split-screen camera and input routing. |
| Deterministic iteration order | Entity ID ordering, terrain scan ordering. |
