# Container System

## Overview

Any entity can be a container if `[container].enabled = true` in its definition. Containment is an engine-level concept: contained entities are removed from the world (no physics, no rendering) and exist only inside their parent.

## Nesting

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

## Filter

The `filter` field lists category tags. An object can only enter the container if it has **at least one** matching category. Empty filter means accept everything.

```toml
[container]
enabled = true
max_slots = 4
filter = ["Material"]    # only accepts objects with "Material" category
```

## Script Override

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

## Container Engine API

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

## Container Callbacks

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
