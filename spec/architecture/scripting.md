# Scripting (Lua)

## Runtime

Lua 5.4 embedded via **sol2** (header-only C++ binding library). Each aspect's `script.lua` runs in a shared Lua state but with its own environment table, so globals don't leak between aspects.

## Script Structure

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

## The `self` Table

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

## Engine API Categories

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

## Engine Callbacks (Full List)

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
| `OnCollision(other_id, vx, vy)` | Two solid entities physically collide |
| `OnOverlapBegin(other_id)` | AABB overlap starts |
| `OnOverlap(other_id)` | AABB overlap continues (every tick) |
| `OnOverlapEnd(other_id)` | AABB overlap ends |
| `OnHitTerrain(material_id, x, y, vx, vy)` | Entity hits solid terrain during movement |
| `OnLiquidLevelChange(level, liquid_material)` | Liquid ratio inside AABB changes (0.0–1.0) |
| `OnFallImpact(impact_speed)` | Entity lands after being airborne |
