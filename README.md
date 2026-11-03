# N-Type

A small C++20 game engine: an ECS on top of SFML. First target: a bullet hell.

```sh
sudo apt install g++ cmake ninja-build libsfml-dev nlohmann-json3-dev
cmake -S . -B build -G Ninja
cmake --build build
./build/ntype
```

Move the green square with Z, Q, S, D and shoot with Space. Two enemy types,
loaded from `assets/data/enemies.json`, fly and fire patterns of bullets.

**New here? Read [docs/PROJECT_GUIDE.md](docs/PROJECT_GUIDE.md).** It explains the
ECS idea, every file, the enemy data format, the conventions, and how to add
components, systems, factories and enemies.
