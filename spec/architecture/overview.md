# Aeterium — Architecture Overview

## Stack

| Layer | Technology |
|---|---|
| Language | C++17 |
| Window / Input / Audio | SDL2 |
| Text rendering | SDL2_ttf |
| Scripting | Lua 5.4 (sol2 bindings) |
| Config format | TOML (toml++), JSON (nlohmann/json for scenarios) |
| Build system | CMake 3.14+ (find_package, system libs via Homebrew) |
| Platform target (v0.1) | macOS (Apple Silicon + Intel) |

---

## Guiding Principles

1. **Data-driven**: the engine knows nothing about what a "character" or "smelter" is. All game objects are defined by packages (TOML + Lua + sprites). The engine provides systems; packages provide content.
2. **Composition over inheritance**: behavior is added to objects by nesting child aspects (folders with their own scripts), not by subclassing C++ types.
3. **Moddable by default**: the base game ships as a package. Modders create new packages with the same tools and structure. No special API — modders and the base game use the same system.
4. **Simple until proven insufficient**: start with straightforward implementations. Optimize only when profiling shows a bottleneck.

---

## Definition Types

Definition types, each identified by its TOML section header (or JSON for scenarios):

| Type | Section/Format | Purpose |
|---|---|---|
| Package | `[package]` TOML | Metadata for a content package |
| Object | `[object]` TOML | A spawnable game entity (character, building, item, projectile) |
| Aspect | `[aspect]` TOML | A reusable behavior script that attaches to objects or scenarios |
| Procedure | `[procedure]` TOML | A movement/physics mode used by actions |
| Material | `[material]` TOML + optional Lua | Terrain cell type with physics, visuals, and behavior script |
| Background | `[background]` TOML | Visual-only background layer rendered behind terrain |
| Scenario | `scenario.json` | A playable level: map generation, player slots, rule aspects |
