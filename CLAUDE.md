# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

**Dependencies (macOS):** CMake, SDL2, SDL2_ttf, SDL2_image, Lua 5.4 via Homebrew.
**Dependencies (Linux):** Same via apt-get (see CI workflow for exact package names).

```sh
# Native debug build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
./build/juliana                          # loads packages/mapeditor/game by default
./build/juliana <path-to-package>        # load a specific package

# Native release build
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel

# Web build (Emscripten)
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --parallel
cd build-web && python3 -m http.server 8080  # then open http://localhost:8080/juliana.html
```

There is no automated test suite. Testing is done by running the game manually. The `spec/` directory contains architectural documentation, not executable tests.

## Architecture

Juliana is a data-driven 2D sandbox game engine for terrain-digging games (think falling-sand physics + Box2D rigid bodies), designed to power the game "Aetherium." All game content lives in **packages** (TOML definitions + Lua scripts + sprite assets); the C++ engine is content-agnostic.

### Core Loop (`src/core/`)
Fixed 60 Hz simulation tick with interpolated rendering (delta-accumulation pattern). `GameLoop` coordinates all system updates in order: input → terrain simulation → physics → entity update → render.

### Scripting (`src/scripting/`)
Lua 5.4 via sol2 v3.3.0. The engine exposes a Lua API (`engine.log()`, `engine.ui.*`, `engine.scenario.*`, etc.) that packages use to implement all game logic. C++ bindings use `sol::property` rather than member-variable pointers (required for AppleClang 16 compatibility). Lua scripts are loaded per-package at startup; tick callbacks drive ongoing logic.

### Package & Definition System (`src/package/`)
Packages are directories containing a `game.toml` (or `package.toml`) plus subdirectories for objects, materials, backgrounds, aspects, and scenarios. `PackageLoader` reads TOML definitions into a `DefinitionRegistry` that content systems query by name. Packages can nest and inherit from other packages.

### Terrain System (`src/terrain/`)
Cell-based grid storing material + background per cell. Rendered in 64×64 chunks via streaming GPU textures. Simulates falling-sand, liquid, and powder dynamics at fixed tick rate. Player digging carves terrain cells using action-mapped tools.

### UI System (`src/ui/`)
Screen-stack architecture: push/pop named screens. Element tree (buttons, containers, labels) with skin/font loading and callback-driven interaction. Lua scripts build and manage UI screens entirely through the scripting API — the map editor (`packages/mapeditor/`) is implemented purely in Lua using this API.

### Physics (`src/physics/`)
Box2D 2.4.1 integration with fixed-timestep stepping. Manages rigid bodies for entities and terrain fragments.

### Entity System (`src/entity/`)
`EntityManager` handles spawning and lifecycle. Entities use action state machines (`ActionMap`) for animation. Named points on entities serve as attachment sites for behaviors/aspects.

### Input (`src/input/`)
SDL2 event polling translated into an action-binding layer. Per-player slot bindings support multiple simultaneous characters. Keyboard events map to named actions consumed by game logic.

### Scenario System (`src/scenario/`)
JSON-based map definitions with generation seeds, spawn positions, and player slots. `MapGenerator` creates procedural or preset terrain from scenario definitions.

## Key Packages

| Package | Role |
|---------|------|
| `packages/mapeditor/game/` | Default startup package; the map editor app (Lua UI) |
| `packages/aetherium/` | Gameplay scenario; `base/` has reusable content, `game/` is the playable scenario |
| `packages/test_package/` | Minimal content for manual testing |

## Dependencies (FetchContent / vendored)

- **sol2 v3.3.0** — header-only C++ Lua bindings (a sol2 Emscripten bug is patched by `cmake/patch_sol2.py` at configure time)
- **toml++ v3.4.0** — header-only TOML parser
- **nlohmann/json v3.11.3** — header-only JSON
- **Box2D v2.4.1** — rigid body physics (unit tests disabled in CMakeLists.txt to avoid assembly conflicts)
- **Lua 5.4** — compiled from source via FetchContent for web builds; system lib for native

## CI

GitHub Actions (`.github/workflows/build.yml`) builds Release binaries on `ubuntu-24.04` and `macos-14` for every push/PR to main. FetchContent deps are cached by CMakeLists.txt hash. Web builds are not part of CI.
