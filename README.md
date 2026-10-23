# N-Type

A small C++ game engine, written step by step: an ECS on top of SFML.

- **Entity**: just an id (`int`).
- **Components** (`engine/include/Component.hpp`): plain data, such as `E_Position`, `E_Velocity` and `E_Rectangle`.
- **World** (`engine/include/World.hpp`): one map per component type, keyed by entity id.
- **Systems** (`engine/src/System.cpp`): functions over the world: `Move_Input` (ZQSD), `Movement`, `Draw`.
- **Factory** (`engine/src/Factory.cpp`): functions that build entities, such as `Create_E_Player`.

```
engine/include/   headers
engine/src/       systems and factory
src/main.cpp      window and game loop
assets/           sprites, sounds, font
docs/             project subject
```

## Build and run

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/ntype
```

Needs CMake 3.25+, a C++20 compiler and SFML 2.6 (`libsfml-dev`).
Move the green square with Z, Q, S, D.
