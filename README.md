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
Lets fix some things:
Lets fix some things:

* Change InputSystem to work with game concepts, like moveLeft, moveRight, Click possition, that will handle diferent input sources, like, keyboard, mouse, gamepad, touch, and  also deal with key rebinding, lets also create a input manager in a way that the splitscreen player will have its own system(with the diferent mappings)

* Change the camera logic to handle splitscreen players, do not implement splitscreen now, just make the camera logic in a way that works for more than one player and will follow the correct player

* Also add a active character logic, and change character buttons (use `1` previous and `3` next), the camera and controls will be target to the choosen char

* Lets also include something on the character object definition  to indicate that it can be controlled by the player

* 

* Remove the Fixed from the code, let go with normal floats

* Update the documentation accordingly