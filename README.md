# Aeterium

A spiritual clone of Clonk Endeavor — a 2D sandbox game featuring physics-based terrain digging, ore fragmentation, and base building. Built with C++17 and SDL2.

## Prerequisites

- macOS (Apple Silicon or Intel)
- [Homebrew](https://brew.sh)
- Xcode Command Line Tools: `xcode-select --install`
- CMake: `brew install cmake`
- SDL2: `TODO`

## Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

## Run

```sh
./build/aeterium
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

## Tech Stack

- C++17
- SDL2
- CMake 3.14+
