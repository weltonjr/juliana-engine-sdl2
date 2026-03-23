# Aspect System (Composition)

## What is an Aspect

An aspect is a reusable behavior module defined by a `definition.toml` (with `[aspect]`) and a `script.lua`. Aspects attach to objects to compose behavior. At runtime, each aspect becomes a separate Lua script context that shares the parent entity.

## Aspect Definition

```toml
[aspect]
id = "Combat"
name = "Combat System"
description = "HP, damage, death handling"
```

## Aspect Tree

Objects reference aspects by ID. Aspects can contain inline child aspects, forming a tree:

```
Character (root — object script.lua)
├── Combat (aspect)
├── Inventory (aspect)
│   └── InventoryUI (inline child aspect of Inventory)
└── Mage (aspect)
    └── CasterUI (inline child aspect of Mage)
```

All aspects in the tree:
- Share the same entity ID
- Can read/write the entity's properties
- Can call engine APIs on the entity (move, get position, set action, etc.)
- Receive engine events (OnTick, OnDamage, OnContentsAdd, etc.)

## On Disk — Inline vs Public Aspects

An object can include aspects two ways:

**Public aspect** — lives at package level, referenced by ID:
```
packages/base/
├── aspects/
│   └── Combat/
│       ├── definition.toml    # [aspect] id = "Combat"
│       └── script.lua
└── characters/
    └── Character/
        ├── definition.toml    # aspects = ["Combat"]
        └── script.lua
```

**Inline aspect** — lives inside the object folder, private to that object:
```
packages/base/
└── characters/
    └── Character/
        ├── definition.toml    # aspects = ["CharacterUI"]
        ├── script.lua
        └── CharacterUI/
            ├── definition.toml    # [aspect] id = "CharacterUI"
            └── script.lua
```

## Event Dispatch Order

Engine events are dispatched **depth-first, top-down** through the aspect tree:

```
OnTick → Character → Combat → Inventory → InventoryUI → Mage → CasterUI
```

Any aspect can consume an event by returning `true`, which stops propagation to later siblings and children.

## Event Bus (Messaging)

Aspects communicate via a message bus scoped to the entity:

```lua
-- In Combat/script.lua
function OnDamage(amount, source)
    self.hp = self.hp - amount
    if self.hp <= 0 then
        SendMessage("died", { killer = source })
    else
        SendMessage("hurt", { amount = amount })
    end
end

-- In Mage/CasterUI/script.lua
function OnMessage(msg, data)
    if msg == "hurt" then
        ShowDamageNumber(data.amount)
    elseif msg == "died" then
        HideCasterUI()
    end
end
```

`SendMessage(name, data)` delivers to **all** aspects on the same entity (including self), in tree order. The sender can be identified via `data` if needed.
