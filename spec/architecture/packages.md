# Packages

## Package Definition

Every package has a `definition.toml` at its root:

```toml
[package]
id = "western"
name = "Wild West"
version = "1.0.0"
description = "Cowboys, horses, and gold rushes"
icon = "icon.png"
depends = ["base"]
```

## Folder Structure

Packages have **no enforced folder layout**. The engine scans the entire package tree recursively, finds every `definition.toml`, and reads the section header to determine the type. Folders without a `definition.toml` are organizational — the engine skips them and keeps scanning their children.

```
packages/western/
├── definition.toml                    # [package]
├── icon.png
│
├── characters/                        # organizational folder — no definition.toml, ignored
│   ├── Cowboy/
│   │   ├── definition.toml            # [object]
│   │   ├── script.lua
│   │   ├── sprite.png
│   │   ├── animations.toml
│   │   └── LassoAim/                  # inline aspect — private to Cowboy
│   │       ├── definition.toml        # [aspect]
│   │       └── script.lua
│   └── Sheriff/
│       ├── definition.toml            # [object]
│       └── ...
│
├── aspects/                           # organizational folder
│   ├── Combat/
│   │   ├── definition.toml            # [aspect] — public, any object can reference
│   │   └── script.lua
│   └── Riding/
│       ├── definition.toml            # [aspect]
│       └── script.lua
│
├── buildings/                         # organizational folder
│   └── Saloon/
│       ├── definition.toml            # [object]
│       └── ...
│
└── procedures/                        # organizational folder
    └── lasso_swing/
        ├── definition.toml            # [procedure]
        └── script.lua
```

## Folder Rules

1. Folder without `definition.toml` → organizational, engine skips it and scans children
2. Folder with `definition.toml` → a definition. **No subfolders allowed** except inline aspects (which must have their own `definition.toml` with `[aspect]`)
3. Inline aspects can themselves contain nested inline aspects (aspect tree), but no arbitrary folders inside definitions
4. **Cross-package references** use package prefix: `"base:Combat"`, `"western:Riding"`. Unqualified IDs resolve within the current package first.
5. An inline aspect (nested inside an object folder) is **private** — only the owning object can reference it. Aspects at package level are **public** — any object in any package (with the dependency declared) can reference them.

## Loading Pipeline

```
1. Scan packages/ for definition.toml files with [package], resolve dependency order
2. For each package, recursively scan tree for definition.toml files
3. Read section header → register as object, aspect, procedure, material, or background in DefinitionRegistry
4. For objects: walk subfolders for inline aspects → build aspect tree
5. For objects: load sprite.png, parse animations.toml → attach to definition
6. Pre-compile all Lua scripts (syntax check only; no execution until entity is spawned)
7. Validate references: check that all aspect/procedure IDs referenced by objects exist
   and are accessible (public, or private to that object)

At runtime when Spawn("Character", x, y) is called:
1. Look up "Character" in DefinitionRegistry
2. Allocate entity ID (monotonic uint32_t, never reused)
3. Create physics body from definition
4. Create Lua contexts for root + all aspects
5. Call OnInitialize() depth-first
6. Entity is now live in the world
```
