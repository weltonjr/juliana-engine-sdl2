# Scenario System

## Overview

A scenario is a playable level. It defines how the map is generated, which players participate, what they start with, and which aspects control the rules and environment. Scenarios use **JSON**, generated and edited by the map editor.

Scenario aspects (rules, wildlife, weather) use the same TOML + Lua aspect system as entity aspects, but attach to the **world/session** instead of an entity.

## On Disk

```
packages/base/scenarios/GoldRush/
├── scenario.json              # map gen, players, settings
├── icon.png
├── Freeplay/                  # scenario aspect (inline, private)
│   ├── definition.toml        # [aspect]
│   └── script.lua
└── Wildlife/                  # scenario aspect (inline, private)
    ├── definition.toml        # [aspect]
    └── script.lua
```

Scenario aspects follow the same rules as object aspects:
- Inline aspects (inside the scenario folder) are **private** to that scenario
- Public aspects (at package level) can be referenced by any scenario via ID
- Cross-package references use prefix: `"base:Wildlife"`

## scenario.json

```json
{
    "scenario": {
        "id": "GoldRush",
        "name": "Gold Rush",
        "description": "Dig deep, find gold, build your fortune",
        "icon": "icon.png",
        "packages": ["base"],
        "aspects": ["Freeplay", "Wildlife"]
    },

    "map": {
        "width": 2048,
        "height": 512,
        "seed": 0,

        "shape": "island",
        "shape_params": {
            "sea_level": 0.6,
            "terrain_height": 0.4,
            "roughness": 0.5
        },

        "materials": [
            { "id": "base:Air", "rule": "above_surface" },
            { "id": "base:Water", "rule": "below_sea_level_and_empty" },
            { "id": "base:Dirt", "rule": "surface_layer", "depth": 30 },
            { "id": "base:Rock", "rule": "deep", "min_depth": 60 }
        ],

        "features": [
            { "type": "caves", "density": 0.05, "min_size": 10, "max_size": 40 },
            { "type": "ore_veins", "material": "base:GoldOre", "zone": "rock", "density": 0.03, "vein_radius": 8 },
            { "type": "lakes", "count": 3, "zone": "surface" }
        ]
    },

    "players": {
        "slots": [
            {
                "type": "required",
                "team": 1,
                "spawn": {
                    "zone": "surface",
                    "constraints": {
                        "min_flat_width": 32,
                        "min_sky_above": 40,
                        "avoid_water": true,
                        "min_player_distance": 200
                    }
                },
                "objects": [
                    {
                        "definition": "base:Character",
                        "contents": [
                            { "definition": "base:Pickaxe" },
                            { "definition": "base:Dynamite", "count": 3 }
                        ]
                    },
                    {
                        "definition": "base:Character",
                        "contents": [
                            { "definition": "base:Pickaxe" },
                            { "definition": "base:Shovel" }
                        ]
                    }
                ]
            },
            {
                "type": "optional",
                "team": 2,
                "spawn": {
                    "zone": "surface",
                    "constraints": {
                        "min_flat_width": 32,
                        "avoid_water": true,
                        "min_player_distance": 200
                    }
                },
                "objects": [
                    {
                        "definition": "base:Character",
                        "contents": [
                            { "definition": "base:Pickaxe" },
                            { "definition": "base:Dynamite", "count": 3 }
                        ]
                    },
                    {
                        "definition": "base:Character"
                    }
                ]
            }
        ]
    }
}
```

## Player Slot Types

| Type | Meaning |
|---|---|
| `required` | Must be filled for the scenario to start. Can be human or AI. |
| `optional` | Can be human, AI, or left empty. |
| `ai` | Always AI-controlled. |
| `none` | Slot is disabled / reserved for future use. |

## Map Generation Pipeline

Map generation runs as a three-pass pipeline, all configured in `scenario.json`:

**Pass 1 — Base Shape**

The `shape` field selects a generator function that outputs a **density map** (0.0 = empty, 1.0 = solid). The `shape_params` tune the shape. Available shapes:

| Shape | Description | Key Params |
|---|---|---|
| `flat` | Horizontal layers — sky, dirt, rock | `surface_level`, `rock_level` |
| `island` | Terrain mass surrounded by water | `sea_level`, `terrain_height`, `roughness` |
| `islands` | Multiple disconnected terrain masses over water | `count`, `sea_level`, `island_size`, `spacing` |
| `floating` | Terrain chunks suspended in air with gaps | `chunk_count`, `chunk_size`, `gap_size`, `altitude` |
| `mountain` | Central peak, slopes down to edges | `peak_height`, `slope_steepness`, `base_width` |
| `bowl` | High edges, low center (crater/volcano rim) | `rim_height`, `floor_depth`, `rim_width` |
| `cavern` | Mostly solid with a large hollow interior | `cavern_size`, `wall_thickness`, `openings` |
| `dungeon` | Solid mass with carved rooms and corridors | `room_count`, `room_size_min`, `room_size_max`, `corridor_width` |

All shapes use noise to add irregularity. The `roughness` param (0.0–1.0) controls how much noise is applied to edges — 0.0 gives clean geometric shapes, 1.0 gives very organic/noisy shapes.

Shapes are **C++ implementations** (they need tight noise/math integration), but new shapes can be added by modders via engine plugins in the future.

**Pass 2 — Material Assignment**

Rules that assign materials to cells based on their position relative to the density map:

| Rule | Assigns material where... |
|---|---|
| `above_surface` | Cell is empty and above the terrain surface |
| `below_sea_level_and_empty` | Cell is empty and below sea_level Y |
| `surface_layer` | Cell is solid and within `depth` pixels of the surface |
| `deep` | Cell is solid and deeper than `min_depth` from the surface |
| `below_depth` | Cell is below absolute Y depth |
| `fill` | Cell is solid (fallback — fills remaining solid cells) |

Rules are evaluated in order. First match wins. The last rule should be a `fill` to catch anything unmatched.

**Pass 3 — Features**

Post-processing passes that carve or add structures to the generated map:

| Feature | Description | Key Params |
|---|---|---|
| `caves` | Carve irregular hollow spaces | `density`, `min_size`, `max_size`, `zone` |
| `ore_veins` | Place ore blobs in solid terrain | `material`, `density`, `vein_radius`, `zone` |
| `lakes` | Fill surface depressions with water | `count`, `zone` |
| `lava_pools` | Fill deep cavities with lava | `count`, `zone` |
| `tunnels` | Carve winding tunnels | `count`, `zone`, `width` |
| `air_pockets` | Small air bubbles (for underwater levels) | `density`, `zone`, `size` |

The `zone` param restricts where the feature applies: `"surface"`, `"underground"`, `"deep"`, `"rock"`, `"all"`.

## Spawn Zone System

Player spawn positions are determined at game start by querying the generated map. The `spawn` field in each slot defines **rules**, not coordinates — since procedural maps are different every time.

| Zone | Finds positions... |
|---|---|
| `surface` | On top of terrain, open sky above |
| `underground` | Inside a cave or tunnel |
| `underwater` | Submerged in water |
| `peak` | Highest terrain point in an area |
| `flat` | Flat ground stretch of minimum width |
| `cave` | Largest open underground space |

Constraints narrow the search:

| Constraint | Meaning |
|---|---|
| `min_flat_width` | Minimum flat ground width (pixels) at the spawn point |
| `min_sky_above` | Minimum clear space above (pixels) |
| `avoid_water` | Don't spawn in or adjacent to water |
| `min_player_distance` | Minimum distance from other players' spawn points |

The engine scans the map for positions matching all constraints, then picks from candidates.

## Scenario Aspects

Scenario aspects control rules, environment, and world events. They work identically to entity aspects but receive **scenario-level callbacks** instead of entity callbacks:

```lua
-- Wildlife/script.lua
function OnGameStart()
    self.spawn_timer = 0
end

function OnTick(dt)
    self.spawn_timer = self.spawn_timer + dt
    if self.spawn_timer >= 10.0 then
        self.spawn_timer = 0
        local pos = FindSpawnPosition("underground", {
            min_cave_size = 20,
            min_player_distance = 100
        })
        if pos then
            Spawn("base:Bat", pos.x, pos.y)
        end
    end
end
```

## Scenario Callbacks

| Callback | When |
|---|---|
| `OnGameStart()` | Map generated, players spawned, game begins |
| `OnTick(dt)` | Every physics tick |
| `OnPlayerJoin(slot_index, player_id)` | A player fills a slot |
| `OnPlayerLeave(slot_index, player_id)` | A player leaves |
| `OnMessage(name, data)` | Message from SendMessage |

## Scenario Loading Sequence

```
1. Read scenario.json
2. Load required packages (resolve dependencies)
3. Generate map:
   a. Run shape generator → density map
   b. Apply material rules → material map
   c. Run feature passes → final terrain
4. Resolve spawn positions for each slot
5. For each filled slot:
   a. Spawn player objects at resolved position
   b. Create contained objects inside them
6. Load scenario aspects, call OnGameStart()
7. Game loop begins
```
