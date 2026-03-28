# Entity System

## Overview

Everything in the game world is an **Entity**: characters, buildings, trees, ore fragments, projectiles, vehicles. Every entity is an instance of an **Object Definition**. Definitions describe the entity's properties, behavior, visuals, aspects, and actions.

## Object Definition (definition.toml)

```toml
[object]
id = "Character"                      # unique within the package
name = "Adventurer"                   # display name
category = ["Living", "Controllable"] # category tags (strings, not bitflags)
aspects = ["Combat", "Inventory", "Mage"]   # referenced aspects (public or private inline)
player_controllable = false           # if true, spawned instances receive player input and camera focus

[physics]
mode = "dynamic"           # "dynamic" (moves), "static" (fixed), "kinematic" (script-driven)
mass = 50                  # affects force calculations and buoyancy
max_fall_speed = 400.0
collision_shape = "aabb"   # "aabb", "polygon" (planned), "pixel_mask" (chunks only)
size = [12, 20]            # bounding box in world pixels
rotation = false           # enable rotation physics (default false)
angular_drag = 0.98        # rotation damping per tick (1.0 = no drag)
bounce_angular_transfer = 0.3  # linear-to-angular velocity on bounce (0–1)
solid = false              # other entities pass through (true for vehicles/special objects)
overlap_detection = true   # fire OnOverlap callbacks when touching other entities
step_up = 4                # max pixels this entity can step over (default 4, max configurable)

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

### player_controllable

When `player_controllable = true` in the `[object]` block, the engine adds all spawned instances of this object to its internal `controllable_entities_` list. The active entity in that list receives player input (via `InputManager`) and is followed by the camera.

Players cycle through controllable entities with the `PrevCharacter` / `NextCharacter` actions (default keys: `1` / `3`). The camera snaps immediately on switch and then resumes smooth following.

Objects without `player_controllable = true` (the default) are never added to the list — they are driven by AI, scripts, or physics only.

## Entity Lifecycle

```
1. LOAD       — Engine reads definition.toml, loads sprites, compiles Lua scripts
2. CREATE     — Engine instantiates entity: allocates ID, creates physics body,
                creates script contexts for root + all child aspects
3. INITIALIZE — Engine calls OnInitialize() on all aspects (depth-first)
4. TICK       — Every physics tick: OnTick(dt) on all aspects
5. EVENTS     — Engine dispatches events as they happen (OnDamage, OnContentsAdd, etc.)
6. DESTROY    — OnDestroy() on all aspects (reverse order), then deallocate
```

## Entity ID

Each entity instance gets a unique `uint32_t` ID at creation. Scripts reference other entities by ID. IDs are never reused during a session (monotonic counter).
