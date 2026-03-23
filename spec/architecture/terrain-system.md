# Terrain System

## Overview

The terrain is a **cell grid** where each cell is 1 pixel. Every cell stores a foreground material and a background material. The foreground has full physics simulation (collision, gravity, liquid flow). The background is visual only — it's revealed when the foreground is removed (e.g., digging dirt inside a mountain reveals the rock wall behind it).

## Cell Data

Each cell is 2 bytes:

| Field | Type | Purpose |
|---|---|---|
| `material_id` | uint8 | Foreground material (0 = air) |
| `background_id` | uint8 | Background visual (0 = none / open sky) |

A 2048×512 map = ~2MB. Cache-friendly for simulation and rendering.

## Material States

Each material has a physics state that determines how the engine simulates it:

| State | Behavior |
|---|---|
| `solid` | Static. Doesn't move. Entities collide with it. Must be dug or blasted to remove. |
| `powder` | Falls due to gravity. Piles up on surfaces. Acts as solid for entity collision. Can be dug easily. |
| `liquid` | Falls due to gravity. Spreads horizontally. Entities can swim in it. Pressure-based simulation (details TBD). |

Gas state is reserved for future implementation.

## Liquid Simulation

Liquid materials use pressure-based simulation. Connected liquid cells form **bodies** that equalize their surface levels, enabling water to flow through U-shaped tunnels and rise to equalize. Detailed algorithm TBD.

## Terrain Modification

The engine provides APIs for modifying terrain at runtime:

```lua
-- Set a single cell's material
SetMaterial(x, y, "base:Dirt")

-- Set a single cell's background
SetBackground(x, y, "base:RockWall")

-- Dig a circular area (removes foreground, reveals background, spawns dig_product)
DigCircle(x, y, radius)

-- Blast a circular area (removes foreground, applies force to nearby entities)
BlastCircle(x, y, radius, power)

-- Fill a circular area with a material
FillCircle(x, y, radius, "base:Rock")

-- Check material at position
local mat = GetMaterial(x, y)
local bg = GetBackground(x, y)

-- Check material properties
local is_solid = IsSolid(x, y)
local is_liquid = IsLiquid(x, y)
```

When terrain is modified:
1. Affected cells update their `material_id`
2. Background is revealed (or replaced if specified)
3. Powder/liquid cells above the removed area are marked for gravity simulation
4. Nearby liquid bodies are flagged for recalculation
5. `dig_product` objects are spawned if the material defines one

## Simulation Order (Per Tick)

```
1. Process powder cells (top-to-bottom scan):
   - If cell below is empty → fall
   - If cell below is occupied → try diagonal (pile behavior)
2. Process liquid cells:
   - Fall into empty space below
   - Spread horizontally at flow_rate
   - Equalize pressure within connected bodies (TBD)
3. Process material scripts:
   - For boundary cells: call OnAdjacentContact() on materials with scripts
   - For cells overlapping entities: call OnEntityContact()
   - Handles interactions (lava touching water), damage, etc.
```
