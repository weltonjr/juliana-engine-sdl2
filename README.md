# Juliana Engine

A 2D sandbox game engine created to make the game Aetherium (inspired by Clonk Endeavor) — featuring physics-based terrain digging, falling powder/liquid simulation, entity management, and a Lua scripting API. Built with C++17 and SDL2, with a WebAssembly target via Emscripten.

## Prerequisites

### Native (macOS)

```sh
brew install cmake sdl2 sdl2_ttf lua@5.4
xcode-select --install   # if not already installed
```

### Native (Windows — MSYS2 ucrt64)

```sh
pacman -S mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-SDL2 \
          mingw-w64-ucrt-x86_64-SDL2_ttf \
          mingw-w64-ucrt-x86_64-lua
```

### Web (Emscripten)

```sh
brew install emscripten   # macOS
# or follow https://emscripten.org/docs/getting_started/downloads.html
```

Lua is compiled from source automatically for the web target — no system Lua needed.

---

## Build

### Native

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Web (WebAssembly)

```sh
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --parallel
```

This produces `build-web/juliana.html`, `juliana.js`, `juliana.wasm`, and `juliana.data` (the embedded game packages).

> **Note:** The first web configure downloads Lua 5.4 source (~2 MB) via FetchContent — this only happens once.

---

## Run

### Native

```sh
./build/juliana                           # loads packages/mapeditor/game
./build/juliana packages/mapeditor/game   # explicit path
```

### Web

The `.html` file must be served over HTTP (browsers block `file://` XHR for `.data` files):

```sh
cd build-web
python3 -m http.server 8080
# open http://localhost:8080/juliana.html
```

---

## Project Structure

```
src/
  core/        Game loop (fixed 60 Hz tick + interpolated render), window, engine coordinator
  input/       SDL2 event polling, action-based input mapping, per-slot player bindings
  ui/          Screen-stack UI system, element tree, button callbacks, skin/font loading
  scripting/   Lua 5.4 integration via sol2 — exposes engine and UI APIs to scripts
  render/      Camera, debug HUD overlay, engine log console
  terrain/     Cell-based terrain, digging, powder/liquid simulation, chunked renderer
  entity/      Entity manager, physics-driven entities, animation action maps
  physics/     Fixed-timestep collision + gravity
  scenario/    Scenario definition loader, map generator, spawn positions
  package/     Content package loader, material/object/background registry
  game/        Game definition (TOML), window and UI config loader

packages/
  mapeditor/   Map editor application (Lua UI, skin, content packages)
  aetherium/   Gameplay scenario package
```

---

## Controls

| Key | Action |
|-----|--------|
| `` ` `` | Toggle engine log console |
| A / D | Move left / right |
| W or Space | Jump |
| Q + S | Dig downward |
| C + A/D | Dig horizontally |
| 1 | Switch to previous character |
| 3 | Switch to next character |
| Escape | Quit |

---

## Lua Scripting API

Game packages control startup via a Lua script declared in `definition.toml`:

```lua
engine.log("hello")                  -- log to stdout and the in-game console
engine.quit()                        -- request shutdown

local screen = engine.ui.create_screen("main")
local btn = screen:add_button("Play", 100, 100, 120, 40)
btn:on_click(function() engine.log("clicked") end)
engine.ui.show_screen(screen)
engine.ui.pop_screen()
```

---

## Tech Stack

| Component | Library |
|-----------|---------|
| Window / rendering | SDL2 |
| Text rendering | SDL2_ttf |
| Scripting | Lua 5.4 + sol2 v3.3.0 |
| Config files | toml++ v3.4.0 |
| JSON | nlohmann/json v3.11.3 |
| Build system | CMake 3.14+ |
| Web target | Emscripten 3.x+ |
