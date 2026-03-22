# Aeterium — Architecture Specification

## Stack

| Layer | Technology |
|---|---|
| Language | C++17 |
| Window / Input / Audio | SDL2 |
| Text rendering | SDL2_ttf |
| Scripting | Lua 5.4 (sol2 bindings) |
| Config format | TOML (toml++ library) |
| Build system | CMake 3.14+ (find_package, system libs via Homebrew) |
| Platform target (v0.1) | macOS (Apple Silicon + Intel) |

---

## Guiding Principles

1. **Data-driven**: the engine knows nothing about what a "character" or "smelter" is. All game objects are defined by packages (TOML + Lua + sprites). The engine provides systems; packages provide content.
2. **Composition over inheritance**: behavior is added to objects by nesting child aspects (folders with their own scripts), not by subclassing C++ types.
3. **Moddable by default**: the base game ships as a package. Modders create new packages with the same tools and structure. No special API — modders and the base game use the same system.
4. **Simple until proven insufficient**: start with straightforward implementations. Optimize only when profiling shows a bottleneck.

---

## Definition Types

There are four definition types, each identified by its TOML section header:

| Type | Section | Purpose |
|---|---|---|
| Package | `[package]` | Metadata for a content package |
| Object | `[object]` | A spawnable game entity (character, building, item, projectile) |
| Aspect | `[aspect]` | A reusable behavior script that attaches to objects |
| Procedure | `[procedure]` | A movement/physics mode used by actions |

---

## Packages

### Package Definition

Every package has a `definition.toml` at its root:

```toml
[package]
id = "western"
name = "Wild West"
version = "1.0.0"
description = "Cowboys, horses, and gold rushes"
icon = "icon.png"
depends = ["base"]
```

### Folder Structure

Packages have **no enforced folder layout**. The engine scans the entire package tree recursively, finds every `definition.toml`, and reads the section header to determine the type. Folders without a `definition.toml` are organizational — the engine skips them and keeps scanning their children.

```
packages/western/
├── definition.toml                    # [package]
├── icon.png
│
├── characters/                        # organizational folder — no definition.toml, ignored
│   ├── Cowboy/
│   │   ├── definition.toml            # [object]
│   │   ├── script.lua
│   │   ├── sprite.png
│   │   ├── animations.toml
│   │   └── LassoAim/                  # inline aspect — private to Cowboy
│   │       ├── definition.toml        # [aspect]
│   │       └── script.lua
│   └── Sheriff/
│       ├── definition.toml            # [object]
│       └── ...
│
├── aspects/                           # organizational folder
│   ├── Combat/
│   │   ├── definition.toml            # [aspect] — public, any object can reference
│   │   └── script.lua
│   └── Riding/
│       ├── definition.toml            # [aspect]
│       └── script.lua
│
├── buildings/                         # organizational folder
│   └── Saloon/
│       ├── definition.toml            # [object]
│       └── ...
│
└── procedures/                        # organizational folder
    └── lasso_swing/
        ├── definition.toml            # [procedure]
        └── script.lua
```

### Folder Rules

1. Folder without `definition.toml` → organizational, engine skips it and scans children
2. Folder with `definition.toml` → a definition. **No subfolders allowed** except inline aspects (which must have their own `definition.toml` with `[aspect]`)
3. Inline aspects can themselves contain nested inline aspects (aspect tree), but no arbitrary folders inside definitions
4. **Cross-package references** use package prefix: `"base:Combat"`, `"western:Riding"`. Unqualified IDs resolve within the current package first.
5. An inline aspect (nested inside an object folder) is **private** — only the owning object can reference it. Aspects at package level are **public** — any object in any package (with the dependency declared) can reference them.

### Loading Pipeline

```
1. Scan packages/ for definition.toml files with [package], resolve dependency order
2. For each package, recursively scan tree for definition.toml files
3. Read section header → register as object, aspect, or procedure in DefinitionRegistry
4. For objects: walk subfolders for inline aspects → build aspect tree
5. For objects: load sprite.png, parse animations.toml → attach to definition
6. Pre-compile all Lua scripts (syntax check only; no execution until entity is spawned)
7. Validate references: check that all aspect/procedure IDs referenced by objects exist
   and are accessible (public, or private to that object)

At runtime when Spawn("Character", x, y) is called:
1. Look up "Character" in DefinitionRegistry
2. Allocate entity ID (monotonic uint32_t, never reused)
3. Create physics body from definition
4. Create Lua contexts for root + all aspects
5. Call OnInitialize() depth-first
6. Entity is now live in the world
```

---

## Entity System

### Overview

Everything in the game world is an **Entity**: characters, buildings, trees, ore fragments, projectiles, vehicles. Every entity is an instance of an **Object Definition**. Definitions describe the entity's properties, behavior, visuals, aspects, and actions.

### Object Definition (definition.toml)

```toml
[object]
id = "Character"                      # unique within the package
name = "Adventurer"                   # display name
category = ["Living", "Controllable"] # category tags (strings, not bitflags)
aspects = ["Combat", "Inventory", "Mage"]   # referenced aspects (public or private inline)

[physics]
mode = "dynamic"           # "dynamic" (moves), "static" (fixed), "kinematic" (script-driven)
gravity = true
mass = 50
max_fall_speed = 400.0
collision_shape = "aabb"   # "aabb" for now; "polygon" planned
size = [12, 20]            # bounding box in world pixels

[points]
torso = [6, 10]
head = [6, 4]
hand_r = [10, 12]
hand_l = [2, 12]
foot_l = [4, 20]
foot_r = [8, 20]

[container]
enabled = true
max_slots = 8
filter = ["Object", "Tool", "Material"]   # accepted category tags (empty = accept all)

[properties]              # arbitrary key-values accessible from Lua
hp = 100
walk_speed = 120.0
jump_velocity = -280.0
dig_radius = 6
```

### Entity Lifecycle

```
1. LOAD       — Engine reads definition.toml, loads sprites, compiles Lua scripts
2. CREATE     — Engine instantiates entity: allocates ID, creates physics body,
                creates script contexts for root + all child aspects
3. INITIALIZE — Engine calls OnInitialize() on all aspects (depth-first)
4. TICK       — Every physics tick: OnTick(dt) on all aspects
5. EVENTS     — Engine dispatches events as they happen (OnDamage, OnContentsAdd, etc.)
6. DESTROY    — OnDestroy() on all aspects (reverse order), then deallocate
```

### Entity ID

Each entity instance gets a unique `uint32_t` ID at creation. Scripts reference other entities by ID. IDs are never reused during a session (monotonic counter).

---

## Aspect System (Composition)

### What is an Aspect

An aspect is a reusable behavior module defined by a `definition.toml` (with `[aspect]`) and a `script.lua`. Aspects attach to objects to compose behavior. At runtime, each aspect becomes a separate Lua script context that shares the parent entity.

### Aspect Definition

```toml
[aspect]
id = "Combat"
name = "Combat System"
description = "HP, damage, death handling"
```

### Aspect Tree

Objects reference aspects by ID. Aspects can contain inline child aspects, forming a tree:

```
Character (root — object script.lua)
├── Combat (aspect)
├── Inventory (aspect)
│   └── InventoryUI (inline child aspect of Inventory)
└── Mage (aspect)
    └── CasterUI (inline child aspect of Mage)
```

All aspects in the tree:
- Share the same entity ID
- Can read/write the entity's properties
- Can call engine APIs on the entity (move, get position, set action, etc.)
- Receive engine events (OnTick, OnDamage, OnContentsAdd, etc.)

### On Disk — Inline vs Public Aspects

An object can include aspects two ways:

**Public aspect** — lives at package level, referenced by ID:
```
packages/base/
├── aspects/
│   └── Combat/
│       ├── definition.toml    # [aspect] id = "Combat"
│       └── script.lua
└── characters/
    └── Character/
        ├── definition.toml    # aspects = ["Combat"]
        └── script.lua
```

**Inline aspect** — lives inside the object folder, private to that object:
```
packages/base/
└── characters/
    └── Character/
        ├── definition.toml    # aspects = ["CharacterUI"]
        ├── script.lua
        └── CharacterUI/
            ├── definition.toml    # [aspect] id = "CharacterUI"
            └── script.lua
```

### Event Dispatch Order

Engine events are dispatched **depth-first, top-down** through the aspect tree:

```
OnTick → Character → Combat → Inventory → InventoryUI → Mage → CasterUI
```

Any aspect can consume an event by returning `true`, which stops propagation to later siblings and children.

### Event Bus (Messaging)

Aspects communicate via a message bus scoped to the entity:

```lua
-- In Combat/script.lua
function OnDamage(amount, source)
    self.hp = self.hp - amount
    if self.hp <= 0 then
        SendMessage("died", { killer = source })
    else
        SendMessage("hurt", { amount = amount })
    end
end

-- In Mage/CasterUI/script.lua
function OnMessage(msg, data)
    if msg == "hurt" then
        ShowDamageNumber(data.amount)
    elseif msg == "died" then
        HideCasterUI()
    end
end
```

`SendMessage(name, data)` delivers to **all** aspects on the same entity (including self), in tree order. The sender can be identified via `data` if needed.

---

## Named Points

### Purpose

Named points are positions relative to the entity's local origin. They serve as **attachment anchors** — places where things connect visually or logically. They have no physics role.

### Engine API

```lua
-- Returns world-space position of the point, accounting for entity position,
-- facing direction (flips X), and current action frame offset if any.
local x, y = GetPoint("hand_r")

-- Attach a visual (armor, hat) to a point — engine updates position each frame
AttachToPoint("torso", armor_entity_id)

-- Spawn a projectile at a point
local arrow = Spawn("Arrow", GetPoint("hand_r"))

-- Connect a rope between two entity points
CreateRope(pole_id, "rope_hook", character_id, "hand_r")
```

### Point Flipping

When an entity faces left, the engine mirrors point X coordinates around the entity center:
```
flipped_x = entity_width - point_x
```

Scripts always define points for the **right-facing** orientation. The engine handles the flip.

### Action Point Overrides

Actions can override point positions (see Action Map section). A crouching action can shift all points down; an attack action can extend the hand forward.

---

## Container System

### Overview

Any entity can be a container if `[container].enabled = true` in its definition. Containment is an engine-level concept: contained entities are removed from the world (no physics, no rendering) and exist only inside their parent.

### Nesting

Containment nesting is **unlimited**. A character contains a backpack; the backpack contains ore:

```
World
├── Character (entity)
│   ├── Pickaxe (contained)
│   └── Backpack (contained)
│       ├── GoldOre (contained)
│       └── GoldOre (contained)
├── Smelter (entity)
│   └── GoldOre (contained — being processed)
```

### Filter

The `filter` field lists category tags. An object can only enter the container if it has **at least one** matching category. Empty filter means accept everything.

```toml
[container]
enabled = true
max_slots = 4
filter = ["Material"]    # only accepts objects with "Material" category
```

### Script Override

Scripts can implement `OnContainerQuery(entering_object)` for custom accept/reject logic beyond the category filter:

```lua
-- Smelter only accepts ore when not already processing
function OnContainerQuery(obj)
    if self.is_processing then
        return false
    end
    return HasCategory(obj, "Ore")
end
```

### Container Engine API

```lua
-- Move an entity into this container (respects filter + OnContainerQuery)
local success = Enter(entity_id, container_id)

-- Remove entity from its container, place back in world at position
Exit(entity_id, world_x, world_y)

-- Get list of contained entity IDs
local items = Contents(container_id)

-- Get contained entities filtered by category
local ores = Contents(container_id, "Ore")

-- Get count
local n = ContentsCount(container_id)

-- Programmatic add: create object directly inside container (for crafting output)
local ingot = CreateInside("GoldIngot", smelter_id)

-- Programmatic remove: destroy a contained object
Destroy(entity_id)

-- Get the container an entity is inside (nil if in world)
local parent = GetContainer(entity_id)
```

### Container Callbacks

```lua
-- Called on the CONTAINER when an object enters
function OnContentsAdd(obj_id)
end

-- Called on the CONTAINER when an object exits
function OnContentsRemove(obj_id)
end

-- Called on the ENTERING OBJECT
function OnEnterContainer(container_id)
end

-- Called on the EXITING OBJECT
function OnExitContainer(container_id)
end
```

---

## Procedure System

### Overview

Procedures are **movement/physics modes** that control how an entity moves each tick. Instead of hardcoding movement types in C++, procedures are definitions (TOML + Lua) that live in packages.

Actions reference a procedure by ID. When the engine ticks an entity, it runs the procedure associated with the current action.

### Procedure Definition

```toml
# procedures/walk/definition.toml
[procedure]
id = "walk"
name = "Ground Walk"
description = "Horizontal ground movement with step-up traversal"
engine_impl = "walk"    # optional — delegates core logic to C++
```

### Hybrid Model (C++ + Lua)

Some procedures need tight physics integration (collision resolution, terrain probing). These declare `engine_impl` to delegate the heavy lifting to C++, while still allowing Lua customization via hooks.

**With `engine_impl`** (hybrid — C++ core + Lua hooks):

```lua
-- procedures/walk/script.lua

-- Called before the C++ implementation runs
function OnBeforeProcedure(entity, dt)
    -- e.g., modify friction based on terrain
    if GetMaterialBelow() == "ICE" then
        SetProperty("friction", 0.1)
    end
end

-- Called after the C++ implementation runs
function OnAfterProcedure(entity, dt)
    -- e.g., spawn footstep particles
    if IsOnGround() and GetVelocityX() ~= 0 then
        SpawnParticle("dust", GetPoint("foot_l"))
    end
end
```

**Without `engine_impl`** (pure Lua — for modder-created procedures):

```lua
-- procedures/jetpack/script.lua
function OnProcedureTick(entity, dt)
    if IsPressed("jump") then
        local fuel = GetProperty("fuel")
        if fuel > 0 then
            SetVelocityY(GetVelocityY() - 300 * dt)
            SetProperty("fuel", fuel - dt * 10)
            SpawnParticle("flame", GetPoint("foot_l"))
        end
    end
end
```

### Built-in C++ Procedures

The engine ships with a small set of built-in procedures for movement modes that require tight physics integration. These are still definitions in the base package — not invisible engine magic.

| Procedure | Engine Behavior |
|---|---|
| `walk` | Horizontal input → velocity, ground friction, step-up traversal, slope handling |
| `flight` | Airborne physics, gravity, air control (reduced horizontal input), no step-up |
| `swim` | 2D directional input, buoyancy (upward force), water drag |
| `scale` | Wall surface attachment, vertical movement, corner transitions |
| `hangle` | Ceiling attachment, horizontal movement, gap detection |
| `dig` | Immobile, direction from input, removes terrain cells per tick within radius |
| `push` | Like walk but reduced speed, moves target entity along |
| `none` | Engine does nothing — position controlled entirely by script |

### Procedure Execution Order

Each physics tick, for each entity:

```
1. Determine current action → look up procedure ID
2. If procedure has engine_impl:
   a. Call OnBeforeProcedure(entity, dt)  [Lua]
   b. Run C++ implementation              [engine]
   c. Call OnAfterProcedure(entity, dt)   [Lua]
3. If procedure is pure Lua:
   a. Call OnProcedureTick(entity, dt)    [Lua]
4. Run collision resolution               [engine — always]
```

---

## Action Map (Animations & State Machine)

### Overview

The action map replaces hardcoded FSMs. Each object definition declares its actions in `animations.toml`. Actions define animation frames, transitions, procedure, and definition overrides. The engine plays animations and runs procedures; scripts trigger transitions.

### animations.toml

```toml
[actions.Idle]
row = 0                  # row in sprite sheet
frames = 1               # number of frames in the row
delay = 1                # ticks per frame
next = "Idle"            # action to transition to when complete
length = 0               # 0 = indefinite (waits for script to change action)
procedure = "none"       # no movement

[actions.Walk]
row = 1
frames = 8
delay = 3
next = "Walk"            # loops
procedure = "walk"

[actions.Jump]
row = 2
frames = 4
delay = 2
next = "Fall"            # auto-transitions to Fall
procedure = "flight"

[actions.Fall]
row = 3
frames = 2
delay = 4
next = "Fall"
procedure = "flight"

[actions.Swim]
row = 7
frames = 6
delay = 4
next = "Swim"
procedure = "swim"

[actions.Dig]
row = 4
frames = 6
delay = 4
next = "Idle"
length = 24              # total ticks, then auto-transition to next
procedure = "dig"

[actions.Build]
row = 5
frames = 12
delay = 5
next = "Idle"
length = 60
procedure = "none"
```

### Action Overrides

Actions can override **any non-immutable field** from the object's `definition.toml`. When the engine switches to an action, it resets all values to the definition defaults, then applies the action's overrides. Scripts read effective values via `GetProperty()` without knowing if the value came from the base or the action.

**Immutable fields** (engine rejects overrides): `object.id`, `object.name`, `object.category`, `object.aspects`, `container.enabled`, `container.max_slots`, `container.filter`.

Everything else is overridable — mass, physics params, properties, points.

```toml
[actions.Swim]
row = 7
frames = 6
delay = 4
next = "Swim"
procedure = "swim"

[actions.Swim.overrides]
"physics.mass" = 30
"physics.max_fall_speed" = 200.0
"properties.walk_speed" = 60.0

[actions.Crouch]
row = 8
frames = 1
delay = 1
next = "Crouch"
procedure = "walk"

[actions.Crouch.overrides]
"physics.size" = [12, 14]           # smaller hitbox
"properties.walk_speed" = 40.0
"points.head" = [6, 2]              # shifted points
"points.torso" = [6, 8]

[actions.Dig]
row = 4
frames = 6
delay = 4
next = "Idle"
length = 24
procedure = "dig"

[actions.Dig.overrides]
"properties.walk_speed" = 0.0
```

### Keyframes

Specific frames within an action can trigger named events. The engine calls `OnActionKeyframe(action, frame, event_name)` on all aspects.

```toml
[actions.SwordSwing]
row = 6
frames = 8
delay = 3
next = "Idle"
procedure = "none"

[actions.SwordSwing.keyframes]
3 = "hit"
7 = "recover"

[actions.Build]
row = 5
frames = 12
delay = 5
next = "Idle"
procedure = "none"

[actions.Build.keyframes]
5 = "consume"
11 = "place"
```

```lua
function OnActionKeyframe(action, frame, event)
    if action == "SwordSwing" and event == "hit" then
        local x, y = GetPoint("hand_r")
        local targets = FindEntities(x, y, 16, "Living")
        for _, id in ipairs(targets) do
            DealDamage(id, 25, GetID())
        end
    end
end
```

### Per-Frame Callback

For scripts that need fine-grained control, `OnActionFrame(action, frame)` fires on every frame change. Most scripts only need keyframes.

```lua
function OnActionFrame(action, frame)
    if action == "Walk" and (frame == 2 or frame == 6) then
        PlaySound("footstep")
    end
end
```

### Engine API

```lua
-- Set the entity's current action (validates it exists in the action map)
-- Resets overrides to defaults, then applies new action's overrides
SetAction("Walk")

-- Get current action name
local action = GetAction()   -- "Walk"

-- Get current frame number within the action
local frame = GetActionFrame()  -- 0..frames-1
```

### Engine Callbacks

| Callback | When |
|---|---|
| `OnActionComplete(action_name)` | Action finished (length reached or last frame of non-looping action) |
| `OnActionKeyframe(action, frame, event)` | Frame with a keyframe entry is reached |
| `OnActionFrame(action, frame)` | Any frame change (opt-in — only called if script defines it) |

### How It Replaces Hardcoded FSM

The engine handles: advancing frames, looping, auto-transitioning via `next`, applying overrides, and running procedures. All **gameplay transition logic** lives in Lua:

```lua
-- Character/script.lua
function OnTick(dt)
    local action = GetAction()
    local on_ground = IsOnGround()
    local vx, vy = GetVelocity()

    if action == "Jump" and vy > 0 then
        SetAction("Fall")
    elseif action == "Fall" and on_ground then
        SetAction("Idle")
    end

    if IsPressed("move_right") or IsPressed("move_left") then
        if on_ground and action ~= "Dig" then
            SetAction("Walk")
        end
    elseif on_ground and action == "Walk" then
        SetAction("Idle")
    end

    if IsJustPressed("jump") and on_ground then
        SetAction("Jump")
        SetVelocityY(-280)
    end
end
```

---

## Scripting (Lua)

### Runtime

Lua 5.4 embedded via **sol2** (header-only C++ binding library). Each aspect's `script.lua` runs in a shared Lua state but with its own environment table, so globals don't leak between aspects.

### Script Structure

Every script is a flat file with callback functions. No classes, no boilerplate:

```lua
-- Called once when entity is created
function OnInitialize()
    self.hp = GetProperty("hp")
end

-- Called every physics tick (1/60s)
function OnTick(dt)
end

-- Called when this entity takes damage
function OnDamage(amount, source_id)
end

-- Called when entity is about to be destroyed
function OnDestroy()
end
```

### The `self` Table

Each aspect has a `self` table for storing per-instance state. It is **not** shared between aspects — each aspect has its own `self`. To communicate, use `SendMessage`.

```lua
-- Combat/script.lua
function OnInitialize()
    self.hp = GetProperty("hp")
    self.armor = 0
end

-- Inventory/script.lua
function OnInitialize()
    self.selected_slot = 1
end
```

### Engine API Categories

| Category | Examples |
|---|---|
| **Entity** | `GetID()`, `Spawn(def, x, y)`, `Destroy(id)`, `GetDefinition(id)` |
| **Properties** | `GetProperty(key)`, `SetProperty(key, val)` |
| **Position** | `GetPosition()`, `SetPosition(x, y)`, `GetVelocity()`, `SetVelocity(vx, vy)` |
| **Points** | `GetPoint(name)`, `AttachToPoint(name, id)` |
| **Actions** | `SetAction(name)`, `GetAction()`, `GetActionFrame()` |
| **Container** | `Enter(id, container)`, `Exit(id, x, y)`, `Contents(id)`, `CreateInside(def, container)` |
| **Terrain** | `GetMaterial(x, y)`, `SetMaterial(x, y, mat)`, `DigCircle(x, y, r)` |
| **Input** | `IsPressed(action)`, `IsJustPressed(action)`, `IsReleased(action)` |
| **Messaging** | `SendMessage(name, data)` |
| **Timer** | `Schedule(ticks, callback)`, `CancelSchedule(id)` |
| **Query** | `FindEntities(x, y, radius, category)`, `FindNearest(x, y, category)` |
| **Camera** | `SetCameraTarget(id)`, `ShakeCamera(intensity, duration)` |
| **Audio** | `PlaySound(name)`, `PlayMusic(name)` |
| **UI** | `ShowMessage(text)` — minimal for now, expanded later |

### Engine Callbacks (Full List)

| Callback | When |
|---|---|
| `OnInitialize()` | Entity created, after all aspects loaded |
| `OnDestroy()` | Entity about to be removed |
| `OnTick(dt)` | Every physics tick |
| `OnDamage(amount, source_id)` | Entity receives damage |
| `OnActionComplete(action_name)` | Action finished |
| `OnActionKeyframe(action, frame, event)` | Keyframe reached |
| `OnActionFrame(action, frame)` | Any frame change (opt-in) |
| `OnContentsAdd(obj_id)` | Object entered this container |
| `OnContentsRemove(obj_id)` | Object left this container |
| `OnEnterContainer(container_id)` | This entity entered a container |
| `OnExitContainer(container_id)` | This entity left a container |
| `OnContainerQuery(obj_id) → bool` | Filter check for entering object |
| `OnMessage(name, data)` | Message from SendMessage |
| `OnCollision(other_id)` | Physics collision with another entity |
| `OnHitTerrain(material, x, y)` | Hit solid terrain during movement |
