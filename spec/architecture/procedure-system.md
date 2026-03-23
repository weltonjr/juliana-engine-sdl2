# Procedure System

## Overview

Procedures are **movement/physics modes** that control how an entity moves each tick. Instead of hardcoding movement types in C++, procedures are definitions (TOML + Lua) that live in packages.

Actions reference a procedure by ID. When the engine ticks an entity, it runs the procedure associated with the current action.

## Procedure Definition

```toml
# procedures/walk/definition.toml
[procedure]
id = "walk"
name = "Ground Walk"
description = "Horizontal ground movement with step-up traversal"
engine_impl = "walk"    # optional — delegates core logic to C++
```

## Hybrid Model (C++ + Lua)

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

## Built-in C++ Procedures

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

## Procedure Execution Order

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
