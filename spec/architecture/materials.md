# Material Definitions

## Overview

A material is a definition type that describes a terrain cell's physics, visual, and behavior. Materials are defined in packages like all other definitions.

## On Disk

```
packages/base/materials/
├── Dirt/
│   ├── definition.toml      # [material]
│   ├── script.lua           # optional — behavior script
│   └── dirt.png             # optional — texture tile
├── Rock/
│   ├── definition.toml
│   └── rock.png
├── Water/
│   ├── definition.toml
│   └── script.lua
├── Lava/
│   ├── definition.toml
│   └── script.lua
└── Sand/
    └── definition.toml
```

## definition.toml Examples

```toml
# Dirt — basic solid terrain
[material]
id = "Dirt"
name = "Dirt"

[physics]
state = "solid"
density = 100                # used for sorting: heavier sinks below lighter
friction = 0.8               # surface friction for entities
hardness = 30                # dig difficulty (ticks per cell)

[visual]
color = [139, 119, 101]      # base render color
texture = "dirt.png"         # optional tiling texture
color_variation = 10         # random RGB offset per cell for natural look

[behavior]
gravity = false              # solid dirt doesn't fall
flammable = false
blast_resistance = 20
dig_product = "base:Earth"   # object spawned when dug
```

```toml
# Rock — hard solid terrain
[material]
id = "Rock"
name = "Rock"

[physics]
state = "solid"
density = 200
friction = 0.9
hardness = 80

[visual]
color = [128, 128, 128]
texture = "rock.png"
color_variation = 8

[behavior]
gravity = false
flammable = false
blast_resistance = 80
dig_product = "base:Stone"   # requires explosives to break, then drops stone
```

```toml
# Sand — powder that falls and piles
[material]
id = "Sand"
name = "Sand"

[physics]
state = "powder"
density = 90
friction = 0.6
hardness = 10

[visual]
color = [210, 190, 140]
color_variation = 12

[behavior]
gravity = true
flammable = false
blast_resistance = 5
dig_product = "base:SandObj"
```

```toml
# Water — liquid
[material]
id = "Water"
name = "Water"

[physics]
state = "liquid"
density = 50
friction = 0.2
hardness = 0                 # cannot dig water

[visual]
color = [60, 100, 200]
transparency = 0.5
color_variation = 5

[behavior]
gravity = true
flow_rate = 3                # horizontal spread speed (cells per tick)
liquid_drag = 0.85           # velocity reduction while submerged
```

```toml
# Lava — dangerous liquid
[material]
id = "Lava"
name = "Lava"

[physics]
state = "liquid"
density = 80
friction = 0.4
hardness = 0

[visual]
color = [255, 80, 20]
transparency = 0.3
glow = true                  # emits light
color_variation = 15

[behavior]
gravity = true
flow_rate = 1
liquid_drag = 0.95           # very high drag
```

```toml
# GoldOre — solid ore vein
[material]
id = "GoldOre"
name = "Gold Ore"

[physics]
state = "solid"
density = 180
friction = 0.9
hardness = 60

[visual]
color = [200, 170, 50]
texture = "gold_ore.png"
color_variation = 10

[behavior]
gravity = false
flammable = false
blast_resistance = 60
dig_product = "base:GoldChunk"
```

## Material Scripts

Materials can have an optional `script.lua` that handles behavior — including interactions with other materials, damage to entities, and environmental effects. The script runs per material type, not per cell (the engine batches calls).

```lua
-- Lava/script.lua

-- Called each tick for cells of this material that are adjacent to other materials
-- adjacents is a list of {x, y, material_id} for neighboring cells
function OnAdjacentContact(x, y, adjacents)
    for _, adj in ipairs(adjacents) do
        -- Lava + Water = Rock (obsidian) + steam
        if adj.material_id == "base:Water" then
            SetMaterial(adj.x, adj.y, "base:Air")
            SetMaterial(x, y, "base:Rock")
            SpawnParticle("steam", x, y)
            PlaySound("sizzle", x, y)
            return  -- this cell is now rock, stop processing
        end
        -- Lava ignites flammable materials
        if GetMaterialProperty(adj.material_id, "behavior.flammable") then
            SetMaterial(adj.x, adj.y, "base:Air")
            SpawnParticle("fire", adj.x, adj.y)
        end
    end
end

-- Called when an entity overlaps cells of this material
function OnEntityContact(x, y, entity_id)
    DealDamage(entity_id, 50 * GetDeltaTime(), nil)
    SpawnParticle("burn", x, y)
end
```

```lua
-- Water/script.lua

function OnEntityContact(x, y, entity_id)
    -- extinguish burning entities
    SendMessageTo(entity_id, "extinguish", {})
end

function OnAdjacentContact(x, y, adjacents)
    for _, adj in ipairs(adjacents) do
        -- Water dissolves loose dirt over time
        if adj.material_id == "base:LooseDirt" then
            if Random() < 0.01 then
                SetMaterial(adj.x, adj.y, "base:Water")
            end
        end
    end
end
```

## Material Callbacks

| Callback | When |
|---|---|
| `OnAdjacentContact(x, y, adjacents)` | This material cell has a neighbor of a different type. Called in batches per tick. |
| `OnEntityContact(x, y, entity_id)` | An entity overlaps a cell of this material |
| `OnDug(x, y, digger_id)` | A cell of this material was dug by an entity |
| `OnBlasted(x, y)` | A cell of this material was hit by an explosion |

## Performance Note

`OnAdjacentContact` is the expensive callback — it could fire for every liquid/lava surface cell every tick. The engine optimizes by:
- Only calling it for materials that define the callback (if Dirt has no script, it's skipped)
- Only checking cells at **boundaries** (where material changes), not interior cells
- Batching: the engine collects all boundary cells per material type, then calls the script once with the full list (not once per cell)
- Not self iteraction, the engine will not pass a bondary that is of the same material
