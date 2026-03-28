# Named Points

## Purpose

Named points are positions relative to the entity's local origin. They serve as **attachment anchors** — places where things connect visually or logically. They have no physics role.

## Engine API

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

## Point Flipping

When an entity faces left, the engine mirrors point X coordinates around the entity center:
```
flipped_x = entity_width - point_x
```

Scripts always define points for the **right-facing** orientation. The engine handles the flip.

## Action Point Overrides

Actions can override point positions (see Action Map section). A crouching action can shift all points down; an attack action can extend the hand forward.
