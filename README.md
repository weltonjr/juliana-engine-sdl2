# Juliana Engine

A spiritual clone of Clonk Endeavor — a 2D sandbox game featuring physics-based terrain digging, ore fragmentation, and base building. Built with C++17 and SDL2.

## Prerequisites

- macOS (Apple Silicon or Intel)
- [Homebrew](https://brew.sh)
- Xcode Command Line Tools: `xcode-select --install`
- CMake: `brew install cmake`
- SDL2: `TODO`
- TOML: `brew install cmake`

## Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

## Run

```sh
./build/juliana
```

## Project Structure

```
src/
  core/      # Game loop, window management, timing
  terrain/   # Tile map, digging logic, ore fragmentation
  entity/    # Entity definitions, FSM
  scripting/ # Scripting API definitions
  camera/    # Camera follow and viewport management
  input/     # Input polling and action mapping
```

## Controls

| Key | Action |
|-----|--------|
| A / D | Move left / right |
| W or Space | Jump |
| Q + S | Dig downward |
| C + A/D | Dig horizontally |
| 1 | Switch to previous controllable character |
| 3 | Switch to next controllable character |
| Escape | Quit |

Input bindings are action-based (`InputAction`) and can be rebound at runtime via `InputManager::SetBinding(slot, action, binding)`.

## Tech Stack

- C++17
- SDL2
- CMake 3.14+