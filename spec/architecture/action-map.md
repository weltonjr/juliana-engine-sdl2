# Action Map (Animations & State Machine)

## Overview

The action map replaces hardcoded FSMs. Each object definition declares its actions in `animations.toml`. Actions define animation frames, transitions, procedure, and definition overrides. The engine plays animations and runs procedures; scripts trigger transitions.

## animations.toml

```toml
[actions.Idle]
row = 0                  # row in sprite sheet
frames = 1               # number of frames in the row
delay = 3                # ticks per frame
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

## Action Overrides

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

## Keyframes

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

## Per-Frame Callback

For scripts that need fine-grained control, `OnActionFrame(action, frame)` fires on every frame change. Most scripts only need keyframes.

```lua
function OnActionFrame(action, frame)
    if action == "Walk" and (frame == 2 or frame == 6) then
        PlaySound("footstep")
    end
end
```

## Engine API

```lua
-- Set the entity's current action (validates it exists in the action map)
-- Resets overrides to defaults, then applies new action's overrides
SetAction("Walk")

-- Get current action name
local action = GetAction()   -- "Walk"

-- Get current frame number within the action
local frame = GetActionFrame()  -- 0..frames-1
```

## Engine Callbacks

| Callback | When |
|---|---|
| `OnActionComplete(action_name)` | Action finished (length reached or last frame of non-looping action) |
| `OnActionKeyframe(action, frame, event)` | Frame with a keyframe entry is reached |
| `OnActionFrame(action, frame)` | Any frame change (opt-in — only called if script defines it) |

## How It Replaces Hardcoded FSM

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
