# N-Type engine

A small C++20 game engine: an ECS on top of SFML. First target: a bullet hell.

```sh
sudo apt install g++ cmake ninja-build libsfml-dev nlohmann-json3-dev
cmake -S . -B build -G Ninja
cmake --build build
./build/ntype
```

Move the ship with Z, Q, S, D and shoot with Space. Two enemy types are loaded from
`assets/data/enemies.json` and stand still for now.

- **Components** (`engine/include/Component.hpp`): plain data.
- **World** (`engine/include/World.hpp`): one map per component type, keyed by entity id.
- **Systems** (`engine/src/System.cpp`): functions over the world.
- **Factory** (`engine/src/Factory.cpp`): functions that build entities.
- **Loader** (`engine/src/Loader.cpp`): reads the enemy types from JSON.
