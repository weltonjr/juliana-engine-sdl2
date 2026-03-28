# Background Definitions

## Overview

Backgrounds are visual-only materials that render behind the foreground terrain. They are revealed when foreground cells are removed (digging, blasting). They have no physics, no collision, and no simulation.

## On Disk

```
packages/base/backgrounds/
├── DirtWall/
│   ├── definition.toml      # [background]
│   └── dirt_wall.png        # optional texture
├── RockWall/
│   ├── definition.toml
│   └── rock_wall.png
└── Sky/
    └── definition.toml
```

## definition.toml

```toml
# DirtWall — behind dirt terrain
[background]
id = "DirtWall"
name = "Dirt Wall"

[visual]
color = [90, 75, 60]         # darker than foreground dirt
texture = "dirt_wall.png"
color_variation = 6
```

```toml
# RockWall — behind rock terrain
[background]
id = "RockWall"
name = "Rock Wall"

[visual]
color = [80, 80, 85]
texture = "rock_wall.png"
color_variation = 5
```

```toml
# Sky — open sky background (no visual, just marks "outdoors")
[background]
id = "Sky"
name = "Open Sky"

[visual]
color = [0, 0, 0]            # not rendered — sky/parallax shows through
transparent = true
```

## Rendering Order

```
1. Sky / parallax background layers
2. Background cells (where foreground is air and background is not Sky)
3. Foreground terrain cells
4. Entities
5. Particles / effects
6. UI / HUD
```

Background cells render with a darkened tint or a separate texture to visually distinguish "inside a tunnel" from "open air".

## Map Generator Background Assignment

During map generation Pass 2, backgrounds are assigned alongside foreground materials:

- Surface dirt cells with sky above → background = `Sky`
- Dirt cells deep inside terrain → background = `DirtWall`
- Rock cells → background = `RockWall`
- Cells below sea level with water above → background = `Sky` (underwater open areas)

This is configured in the scenario's material rules:

```json
{
    "materials": [
        { "id": "base:Dirt", "rule": "surface_layer", "depth": 30, "background": "base:DirtWall" },
        { "id": "base:Rock", "rule": "deep", "min_depth": 60, "background": "base:RockWall" }
    ]
}
```
