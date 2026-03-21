# Aeterium

A spiritual clone of Clonk Endeavor — a 2D sandbox game featuring physics-based terrain digging, ore fragmentation, and base building. Built with C++17 and raylib.

## Prerequisites

- macOS (Apple Silicon or Intel)
- [Homebrew](https://brew.sh)
- Xcode Command Line Tools: `xcode-select --install`
- CMake: `brew install cmake`
- raylib 5.5: `brew install raylib`

## Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

## Run

```sh
./build/aeterium
```

## Controls (v0.1)

| Input | Action |
|---|---|
| Arrow keys / WASD | Move |
| Space | Jump |
| Hold Z / X | Dig |
| F1 | Toggle debug overlay |
| ESC | Quit |

## Project Structure

```
src/
  core/      # Game loop, window management, timing
  terrain/   # Tile map, digging logic, ore fragmentation
  entity/    # Player and entity definitions, FSM
  camera/    # Camera follow and viewport management
  input/     # Input polling and action mapping
```

## v0.1 Scope

- Terrain rendering with a tile-based map
- Controllable player character driven by a finite state machine
- Real-time terrain digging
- Smooth camera follow

## Tech Stack

- C++17
- [raylib 5.5](https://www.raylib.com) (installed via Homebrew)
- CMake 3.14+
