# Aeterium — Gameplay Specification

## Vision

Aeterium is a spiritual successor to Clonk Endeavor: a 2D sandbox platformer where the
world is a destructible bitmap. The player controls several characters who dig into the earth,
blast ore veins, collect the fragments, refine them, and use the materials to build
structures. And fight against the environment, creatures, AI players, other players on the
internet or split screen.

The engine has complete modding support, and the base game ships as a package — using the
same system modders use. The base game will come with several packages (base, western, jungle,
underwater, pirates). New packages can be downloaded from modders.

The player can choose from several ready scenarios from one of the packages.

---

## Core Loop

1. **Explore** — scout the terrain for ore deposits and good dig sites
2. **Dig** — carve tunnels through dirt, expose rock, reach ore veins
3. **Blast** — use explosives to fracture ore into dislodged terrain chunks
4. **Grab** — walk near a chunk and interact to pick it up (attached to character, not in inventory)
5. **Carry** — transport chunks back to base (movement slowed by chunk mass)
6. **Deliver** — drop chunks at a building's intake area (smelter, refinery)
7. **Refine** — building processes the material over time, produces refined items
8. **Build** — construct new structures on flat terrain (smelters, storage, elevators, platforms)
9. **Expand** — use new structures to dig deeper, access new materials, or survive the environment

## Battle Loop

1. **Core loop** — everything from the core loop
2. **Craft** — craft weapons, armors and vehicles (catapults, balloons, ships)
3. **Build defenses** — build walls, pits, whole castles to protect against dangers
4. **Attack** — attack the enemy base with an army and destroy all enemy characters

---

## Player Character

### Movement

| Action | Input | Notes |
|---|---|---|
| Walk left / right | A / D | Walk procedure handles ground movement, step-up |
| Jump | W | Impulse applied, transitions to Jump action → Fall |
| Dig down | Hold Q + S | Triggers Dig action with dig procedure |
| Dig horizontal | Hold C + moving | Dig action in facing direction |
| Grab / Drop | E near chunk | Grabs a terrain chunk, enters Carry action |
| Interact | E | Context menu / use building |

### Character Actions (ActMap)

Character behavior is driven by the **action map** defined in `animations.toml`. The engine handles animation playback and procedure execution. Scripts handle transition logic.

Key actions:

| Action | Procedure | Behavior |
|---|---|---|
| Idle | none | Standing still on ground |
| Walk | walk | Horizontal movement, step-up traversal, constant speed on slopes |
| Jump | flight | Rising after jump impulse |
| Fall | flight | Airborne with downward velocity |
| Swim | swim | In liquid — engine buoyancy + player control |
| Dig | dig | Removing terrain cells in a direction |
| Build | none | Stopped, building animation |
| Carry | walk | Holding a terrain chunk, speed reduced by chunk mass |
| Scale | scale | Climbing a wall surface |
| Hangle | hangle | Hanging from ceiling |

Transition logic lives in the character's `script.lua`. Example: when `vy > 0` during Jump, transition to Fall. When Fall + on ground, transition to Idle. When liquid level > 0.7, transition to Swim.

### Fall Damage

The engine detects landing impacts and calls `OnFallImpact(impact_speed)`. The character's Combat aspect script decides damage based on configurable thresholds in `[properties]`.

---

## Materials

Materials are **definitions** in packages (`[material]` in `definition.toml`). Each material defines its physics state, visual appearance, and optional behavior script.

### Base Materials

| Material | State | Diggable | Hardness | Notes |
|---|---|---|---|---|
| Air | — | — | — | Empty space |
| Dirt | solid | Yes | 30 | Most common surface terrain |
| Rock | solid | Explosives only | 80 | Structural, requires blasting |
| Sand | powder | Yes (easy) | 10 | Falls and piles up |
| GoldOre | solid | Explosives only | 60 | Drops GoldChunk when blasted |
| CoalOre | solid | Yes (slow) | 40 | Fuel source |
| Water | liquid | — | — | Flows, pressure-based simulation |
| Lava | liquid | — | — | Damages entities on contact, ignites flammable materials |

### Material Interactions

Handled by **material scripts** (`script.lua` attached to the material definition):
- Lava + Water → Rock (obsidian) + steam particles
- Lava + flammable material → material burns away
- Water + loose dirt → dissolves over time
- Water extinguishes burning entities

### Material Products

When terrain is dug or blasted, materials can spawn **object entities** (`dig_product` field). These objects enter the world as dynamic entities that can be picked up (enter a character's container).

---

## Ore Fragmentation & Terrain Chunks

When explosions hit terrain, the engine fractures it into **dislodged chunks** — dynamic entities with pixel-based shapes that tumble and bounce with full physics (including rotation).

### Explosion → Chunks

1. Explosion applies blast damage to terrain cells in radius
2. Fracture lines propagate within each material type (cracks stop at material boundaries)
3. Disconnected regions break off as chunks
4. Each chunk gets velocity (outward from blast center) and angular velocity (spin)
5. Small fragments (below `min_fragment_pixels`) spawn as normal item objects instead (e.g., GoldDust)

### Chunk Properties

- **Single material**: each chunk is one material type (fractures follow material boundaries)
- **Mass**: calculated from pixel count × material density
- **Physics**: full dynamic physics with rotation — chunks tumble, bounce, and settle
- **Persistent**: chunks stay in the world until grabbed or destroyed

### Carrying Chunks

- Player walks near a chunk and presses interact → chunk attaches to player's hand, player enters **Carry** action
- Carry action uses the walk procedure but **speed is reduced** based on chunk mass vs character's carry strength
- Player can still walk and jump while carrying, but heavier chunks slow them more
- Press interact again to drop the chunk

### Delivering to Buildings

Buildings define an **intake area** (named point + radius). When a player drops a chunk near the intake:

1. Building detects the chunk via overlap
2. Building script checks the chunk's material
3. Building consumes the chunk (destroys it) and adds the material value to its internal storage
4. Building processes the material over time (e.g., smelter produces ingots)

This creates the core gameplay loop: blast ore → grab chunks → carry to smelter → produce refined materials → build.

---

## Buildings

- Are **static, non-solid entities** — characters walk through them
- Placed by the player on **flat terrain** (engine checks flatness)
- Have an HP property; take damage from explosions/fire via scripts
- Have a **container** for storage/processing (with category filters)
- Can have **overlap detection** to know when characters enter/exit
- Can emit fields (a mana crystal emits mana field)
- Behavior defined by aspects (e.g., a Smelter aspect processes ore over time)

### Building Placement

The engine provides `CheckFlatness(x, y, width, tolerance)` for placement validation. Buildings are placed at the terrain surface — no foundation cells are written to the terrain.

---

## Container System (Inventory)

All inventory, storage, and crafting uses the engine's **container system**:

- Characters have a container (inventory slots) with a category filter
- Buildings have a container (storage/processing) with a category filter
- Items enter containers via `Enter()`, exit via `Exit()`
- Scripts control what happens when items enter (smelter starts processing)
- Containers can be nested unlimited (character holds a backpack that holds ore)

---

## Fields

The world has **fields** (using a low-resolution vector field) that simulate the flow of
environmental forces (mana, heat, dimensional stability). Fields can be emitted by buildings,
terrain features, or scenario aspects.

---

## Scenarios

Each playable level is a **scenario** — a JSON file with map generation parameters, player slots, and rule aspects.

### Map Types

The map generator supports multiple shapes for varied gameplay:

| Shape | Description |
|---|---|
| Flat | Classic horizontal layers — sky, dirt, rock |
| Island | Terrain mass surrounded by water |
| Islands | Multiple disconnected terrain masses over water |
| Floating | Terrain chunks suspended in air |
| Mountain | Central peak with slopes |
| Bowl | Crater/volcano rim with low center |
| Cavern | Mostly solid with hollow interior |
| Dungeon | Solid with carved rooms and corridors |

### Player Slots

Scenarios define player slots with types: `required`, `optional`, `ai`, `none`. Each slot defines starting objects (characters + inventory contents) and spawn zone rules.

### Scenario Aspects

Rules, wildlife, and environment behavior are controlled by **scenario aspects** — Lua scripts that receive world-level callbacks (`OnGameStart`, `OnTick`, `OnPlayerJoin`).

---

## Multiplayer

- **Split-screen**: multiple players on one machine, each controlling their own characters
- **Online**: server-authoritative, host runs simulation, clients send inputs (future)
- **AI**: AI-controlled player slots, AI scripts drive input
- Input source is transparent to game logic — scripts call `IsPressed()` without knowing if input comes from keyboard, gamepad, network, or AI
