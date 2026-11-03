# N-Type: project guide

Everything a newcomer needs to read, build, and extend this project.
If something here disagrees with the code, the code wins: please fix this file.

## Contents

1. [What this is, and where it stands](#1-what-this-is-and-where-it-stands)
2. [Build and run](#2-build-and-run)
3. [The ECS idea in five minutes](#3-the-ecs-idea-in-five-minutes)
4. [Repository map](#4-repository-map)
5. [The code, file by file](#5-the-code-file-by-file)
6. [The enemy data file](#6-the-enemy-data-file)
7. [Conventions](#7-conventions)
8. [Recipes: how to add things](#8-recipes-how-to-add-things)
9. [Pitfalls](#9-pitfalls)
10. [What is not built yet](#10-what-is-not-built-yet)
11. [Glossary](#11-glossary)

---

## 1. What this is, and where it stands

N-Type is a small C++20 game engine on top of SFML 2.6. Its goal is a base that can
carry very different 2D games, from a bullet hell (many bullets, collisions, patterns)
to an RPG (grid, inventory, dialogue). It uses an **ECS** (Entity-Component-System):
game objects are not classes with behaviour. They are ids with pieces of data attached,
and separate functions act on the data.

The first target is a **bullet hell**. The component list also contains RPG
components, ready for later.

**What works today**

- An 800x600 window at 60 FPS.
- A green square (the player): **Z Q S D** moves it (AZERTY layout), **Space** shoots.
- Two enemy types loaded from `assets/data/enemies.json`, drawn as sprites from a
  shared texture store. The fairy weaves down the screen firing fans of three bullets.
  The midboss sweeps side to side firing rings of sixteen.
- Collisions with damage: the player has 3 HP and one second of protection after a
  hit. Enemies die when their HP reaches 0.
- Bullets expire (lifetime) or are removed when far outside the window.
- Followers (enemies steering toward a parent) exist as a system, but nothing
  spawns them yet.

**What does not work yet**: see [section 10](#10-what-is-not-built-yet). In short:
only a few built-in patterns, no waves, no UI or pause, no sound, no game over.

---

## 2. Build and run

**Requirements**: a C++20 compiler (GCC 13+ or Clang), CMake 3.25+, Ninja,
SFML 2.6 and nlohmann/json.

```sh
# Debian / Ubuntu
sudo apt install g++ cmake ninja-build libsfml-dev nlohmann-json3-dev

cmake -S . -B build -G Ninja
cmake --build build
./build/ntype
```

- Move with **Z Q S D**, shoot with **Space**. Close the window to quit.
- The game finds `assets/` through a path compiled into the binary
  (`NTYPE_ASSETS_DIR`), so it runs from any working directory. If you move the
  repository, re-run the `cmake -S . -B build` step.
- Warnings are on (`-Wall -Wextra -Wpedantic`). Keep the build warning-free.
- SFML 3 will **not** work: its API and CMake targets are different.

---

## 3. The ECS idea in five minutes

Consider a ship, a rock and a bullet. A classic design makes three classes.
ECS makes **none**. Instead:

| Word | What it is | Here |
|---|---|---|
| **Entity** | A number that identifies a thing. Nothing else. | an `int` id |
| **Component** | A bundle of data. No behaviour. | `EPosition`, `EVelocity`, `EHealth`... |
| **System** | A function that loops over entities having certain components. | `Movement`, `Draw`... |

The ship, the rock and the bullet differ only by **which components their id has**.

```
id 0 (player)   EPosition  EVelocity  ESpeed  EHealth  ERectangle  EHurtbox  EPlayer
id 1 (enemy)    EPosition  EVelocity  ESpeed  EHealth  ECircle     EHurtbox  EEnemy  ESpawner  EFlight
id 2 (a rock)   EPosition  ERectangle
```

`Movement` moves everything that has an `EVelocity` and an `EPosition`. The rock
has no velocity, so it is never touched. Nobody wrote "rocks do not move": it just has
no `EVelocity`. To make the rock drift, you add one. No other code changes.

**How the data is stored.** `World` has one `std::unordered_map<int, Component>` per
component type, keyed by entity id. "Does entity 5 have a velocity?" means "does
`Velocities` contain key 5?".

**Why this design.** Behaviour is never locked to a type, so combinations are free (a
flying shooter that explodes needs no new class). Systems are small and each lives in
one place. Data of one kind is stored together.

**Tags.** An empty class (`EPlayer`, `EEnemy`, `EBullet`, `ESolid`) carries no data.
Its presence in the map *is* the information.

---

## 4. Repository map

```
CMakeLists.txt           build description
src/main.cpp             creates the world and window, runs the game loop
engine/include/          headers
  Component.hpp          every component (data only)
  World.hpp              all the component maps + Destroy
  EnemyData.hpp          definition of an enemy type (loaded from JSON)
  Factory.hpp            functions that build entities
  System.hpp             functions that run each frame
  Loader.hpp             JSON and texture loading
  Entity.hpp             an Entity class; NOT USED yet (see section 5.2)
engine/src/              the matching .cpp files
  System.cpp             window, one frame (Action), input, movement, drawing
  Behaviour.cpp          flight, following, bullet patterns (what entities do alone)
  Combat.cpp             collisions, damage, timers, cleanup
assets/
  data/enemies.json      enemy types
  data/game.json         from an earlier version of the project; unused
  sprites/ audio/ fonts/ resources for later (unused by the code today)
docs/
  PROJECT_GUIDE.md       this file
  pdfs/                  the project subject
```

Headers are included with relative paths (`"../include/Foo.hpp"`).

---

## 5. The code, file by file

### 5.1 `Component.hpp`: the data

Every component is a `class` with public members and default values. Components hold
**data only**. A small read-only helper is fine, but anything that changes the world
belongs in a system.

| Group | Components |
|---|---|
| Space | `EPosition`, `EVelocity`, `ESpeed`, `ERotation`, `EParent`, `EFlight` |
| Collision | `EHitbox`, `EHurtbox`, `ELayer`, `ESolid` |
| Drawing | `ERectangle`, `ECircle`, `EConvex`, `ETextureId`, `EAnimation`, `EZIndex` |
| UI | `EText`, `EButton` |
| Identity | `EName`, `EOwnerId`, `EPlayer`, `EEnemy`, `EBullet` |
| Combat | `EHealth`, `EDamage`, `EInvincible`, `ELifetime`, `ESpawner` |
| RPG | `EGridPosition`, `EStats`, `EInventory`, `EDialogue`, `ETrigger` |

Points worth knowing:

- **Position is stored only in `EPosition`.** Shapes keep their own position inside SFML,
  but `Draw` overwrites it from `EPosition` every frame, so `EPosition` is the truth.
- **Hitbox vs hurtbox.** A *hitbox* deals damage (a bullet). A *hurtbox* receives it
  (the player, an enemy). Both are circles: `Radius` plus an `Offset` from the entity's
  position. The player's hurtbox is tiny (radius 3) on purpose: in a bullet hell,
  the part that can be hit is much smaller than the sprite.
- **Layers.** `ELayer` has a `Layer` ("what am I") and a `Mask` ("what can I hit"),
  using the bit constants `LayerPlayer`, `LayerEnemy`, `LayerPlayerBullet`,
  `LayerEnemyBullet`. A hitbox should affect a hurtbox when
  `(hitbox_entity.Mask & hurtbox_entity.Layer) != 0`. This prevents friendly fire.
- **`-1` means "none"** for optional ids (`parentid`, `OId`, pattern ids).
- **`EFlight` / `ESpawner`.** An enemy has them only if its data asks for them.
  There is no "disabled" state: no component, no behaviour.
- **`ETextureId`** is a *handle* (an int) to a texture stored once in `World::Textures`,
  plus the part of the image to draw (`Source`) and a `Scale`. Components never hold SFML
  textures or fonts: they are heavy and would be copied per entity. Fonts have no
  shared store yet.

### 5.2 `Entity.hpp`: not used yet

`Entity` is a small class wrapping an id. Nothing includes it: entities are plain
`int`s everywhere. It stays as a placeholder. When entities need more than an id
(for example a generation counter, to detect a stale id), this is where it grows.
Until then, do not build on it.

### 5.3 `World.hpp`: the storage

`World` owns:

- `NextId`: the counter that hands out entity ids. **Ids are never reused**, so an
  id held somewhere is never silently "taken over" by a new entity.
- `EnemyTypes`: loaded enemy definitions (not per-entity, so `Destroy` ignores them).
- `Textures`, `TexturePaths`, `NextTextureId`: the shared texture store. Each image is
  loaded once; asking for the same path again returns the same id.
- One map per component (about 33).
- `Destroy(int id)`: erases the id from every component map.

Each new component needs two lines in `World`: its map, and its `erase` line in
`Destroy`. Forgetting the second one leaves dead entities' data behind, and nothing
warns you. See [section 8](#8-recipes-how-to-add-things).

### 5.4 `Factory.hpp` / `Factory.cpp`: building entities

A factory function is a *recipe*: it creates an id and attaches the right components.

| Function | Result |
|---|---|
| `CreateE(world)` | A bare id (no components). All other factories start with this. |
| `CreateEPlayer(world, pos)` | Green 32x32 square, 3 HP, speed 200, tiny hurtbox, layer Player, and a spawner (pattern 0, every 0.15 s) that fires while Space is held. |
| `CreateEEnemy(world, pos, enemyid)` | Built from `world.EnemyTypes[enemyid]`: a sprite if the type has a texture, otherwise a circle. Returns **-1** if the id is unknown (nothing is created). |
| `CreateEBullet(world, spawnerid)` | One bullet at the spawner's position. See below. |
| `CreateEEnemyFollower(world, pos, enemyid, parentid)` | An enemy with an `EParent`. A system builds a whole train by calling it repeatedly. |
| `CreateEStatic(world, pos)` | An entity with only a position. |

Details:

- **Bullets.** The *spawner* system decides when and how many; the factory makes **one**.
  The bullet gets a placeholder velocity (up for the player's, down for others);
  the pattern system is expected to overwrite it with the real direction. Whether it
  is a player's or an enemy's bullet is decided by whether the spawner has `EPlayer`.
  This sets its layer and mask.
- **Enemy shape.** Enemies are circles drawn around their position (the circle's origin
  is its centre), so the position is the centre. The player's square is positioned by
  its top-left corner instead, which is why the player's hurtbox has an offset of
  (16, 16): to sit in the middle of the square.
- **Helpers** (`AddCircle`, `AddRectangle`, `AddHurtbox`, `AddHitbox`, `SetLayer`,
  `AddEnemyPatterns`) live in an unnamed namespace at the top of `Factory.cpp`. That
  makes them private to the file, so they cannot clash with other files.

### 5.5 The systems

Systems live in three files. Each is a function over the `World`.

**`System.cpp`: window, frame, input, drawing**

| Function | Does |
|---|---|
| `CreateGame(window, name)` | Opens the window (`WindowWidth` x `WindowHeight`), 60 FPS cap, key repeat off. |
| `Action(world, window, dt, playerid)` | One frame. See the order below. |
| `MoveInput(world, playerid)` | Reads Z Q S D and sets the player's `EVelocity` from its `ESpeed`. |
| `Movement(world, dt)` | For each `EVelocity` with an `EPosition`: `position += velocity * dt`. |
| `Draw(world, window)` | Draws sprites, then rectangles, then circles, at each `EPosition`. |

**`Behaviour.cpp`: what entities do on their own**

| Function | Does |
|---|---|
| `Flight(world, dt)` | For each `EFlight`: advances its time and sets the velocity from the flight pattern. |
| `Follow(world)` | For each `EParent`: steers toward the parent, stopping 40 px away. If the parent is gone, stops. |
| `Spawner(world, dt)` | For each `ESpawner`: counts its timer and fires its attack pattern when the interval is reached. The player's spawner only fires while Space is held. |

**`Combat.cpp`: damage and cleanup**

| Function | Does |
|---|---|
| `Collision(world)` | Tests each hitbox against the hurtboxes its `Mask` allows. A bullet that hits is removed. Damage (`EDamage`) is subtracted from `EHealth`. At 0 HP the entity is destroyed. A hit player gets 1 second of `EInvincible`. |
| `Timers(world, dt)` | Counts down `ELifetime` (destroying at 0) and `EInvincible` (removing it at 0). |
| `Cull(world)` | Destroys bullets and enemies more than 64 px outside the window. |

**The frame, in `Action` order:** window events -> clear -> `MoveInput` -> `Flight` ->
`Follow` -> `Spawner` -> `Movement` -> `Collision` -> `Timers` -> `Cull` -> `Draw` -> display.
Decisions (input, patterns) come first, then the movement they cause, then the
consequences (collisions, expiry), then drawing. `dt` is capped at 0.05 s so a long
stall (dragging the window) does not teleport things.

**Patterns.** A pattern id is just a number. What it does is a `switch` in
`Behaviour.cpp`. The built-in ones:

| Kind | Id | Behaviour |
|---|---|---|
| Attack | 0 | The player's shot: one bullet straight up. |
| Attack | 1 | A fan of 3 bullets (-20, 0, +20 degrees), speed 200, aimed down. |
| Attack | 2 | A ring of 16 bullets, speed 150. |
| Flight | 1 | Weaves: horizontal speed 120 * cos(2t), descends at the entity's `ESpeed`. |
| Flight | 2 | Sweeps side to side with amplitude `3 * ESpeed`; does not descend. |

Any other id does nothing. Angles: 0 degrees points straight down.

`dt` is **delta time**: the seconds since the previous frame. Velocities are in
pixels per second, so a frame's movement is `velocity * dt`. This keeps speed
independent of frame rate.

### 5.6 `Loader.hpp` / `Loader.cpp`: JSON and textures

- `LoadEnemies(world, assetsdir)` reads `<assetsdir>/data/enemies.json` once, at
  startup, into `world.EnemyTypes`, loading the textures it names. It returns `false`
  and prints the reason to `stderr` if the file is missing or malformed. Keys other
  than `id` are optional: a missing one keeps the default from `EnemyData`.
- `LoadTexture(world, path)` loads an image into `world.Textures` and returns its
  id, or -1 (with a message) if the file cannot be loaded. The same path always
  gives the same id, so many enemies share one texture.

### 5.7 `main.cpp`: the game loop

1. Create the `World`.
2. Load the enemy types and their textures from `NTYPE_ASSETS_DIR` (exit with an error if
   that fails).
3. Create the window.
4. Create the player and two enemies.
5. Loop while the window is open: call `Action(world, window, dt, player)`, then
   measure `dt` with an `sf::Clock`.

---

## 6. The enemy data file

`assets/data/enemies.json`:

```json
{
  "enemies": [
    {
      "id": 1,
      "name": "fairy",
      "hp": 10,
      "radius": 14,
      "color": [200, 60, 60],
      "speed": 80,
      "fire_interval": 1.2,
      "attack_pattern": 1,
      "flight_pattern": 1,
      "sprite": { "texture": "sprites/bydos.png", "rect": [5, 6, 21, 24], "scale": 2 }
    }
  ]
}
```

| Key | Meaning | Default |
|---|---|---|
| `id` | **Required.** The number passed to `CreateEEnemy`. | none |
| `name` | Display name (`EName`). | `"enemy"` |
| `hp` | Hit points (`EHealth`). | 1 |
| `radius` | Size of the circle, and of the hurtbox. | 16 |
| `color` | `[r, g, b]`, each 0-255. | white |
| `speed` | Movement speed (`ESpeed`). | 0 |
| `fire_interval` | Seconds between volleys (`ESpawner.Interval`). | 1.0 |
| `attack_pattern` | Id of the bullet pattern. **-1 or absent: the enemy has no `ESpawner`.** | -1 |
| `flight_pattern` | Id of the movement pattern. **-1 or absent: the enemy has no `EFlight`.** | -1 |
| `sprite` | Optional object: `texture` (path relative to `assets/`), `rect` (`[x, y, width, height]` in the image, for a sprite sheet cell) and `scale`. **Absent: the enemy is drawn as a circle of colour `color`.** | none |

The pattern ids select the built-in patterns listed in section 5.5.

---

## 7. Conventions

- **Naming: PascalCase.** Each word starts with a capital, words joined with no
  underscore: `CreateEEnemy`, `NextId`, `FireInterval`. This applies to types,
  functions, members and the world's maps. Parameters and local variables are
  lowercase (`world`, `id`, `pos`, `dt`).
  Component class names start with `E` (`EPosition`). Some JSON keys use
  `snake_case` (`fire_interval`): that is the file's own format.
- **`class` with public data** for components. (`struct` and `class` are the same in C++
  except for default access. We use `class` throughout.)
- **At most 4 parameters per function.** When a function wants more, split it, or
  put the extra work in a system.
- **Declare in the header, define in the `.cpp`.** Never put a function body in a header
  (unless it is `inline` or inside a class), or including it from two files breaks the
  link.
- **`#pragma once`** on the first line of every header.
- **Entities are `int`s.** `-1` is "no entity" / "none".
- **Pass big things by reference** (`World&`, `sf::RenderWindow&`). Without the `&`,
  C++ copies the whole object. For a `World` the change would be lost, and an
  `sf::RenderWindow` cannot be copied at all.
- **Headers do not include what they do not use.** In particular, keep SFML includes to
  the places that need them.
- **Check before you read.** Use `find` and compare to `end()`, not `[]`, when you only
  want to look something up (see [pitfalls](#9-pitfalls)).

---

## 8. Recipes: how to add things

### A new component

1. Add the class in `Component.hpp` (data only, with default values).
2. Add its map in `World.hpp`: `std::unordered_map<int, EFoo> Foos;`
3. Add `Foos.erase(id);` in `World::Destroy`. **Do not skip this.**
4. Give it to entities in the right factory, or have a system read it.

### A new system

1. Declare it in `System.hpp`, define it in `System.cpp`. Take `World&` and, if time
   matters, `float dt`.
2. Loop over the map of the component that *defines* the entity set (for example
   `Velocities`), look up the others with `find`, and skip the entity if one is missing.
3. Call it from `Action`, in the right order (input, then logic, then drawing).

```cpp
void Movement(World& world, float dt) {
    for (auto& [id, vel] : world.Velocities) {
        auto it = world.Positions.find(id);
        if (it != world.Positions.end()) {
            it->second.Xy += vel.Xy * dt;
        }
    }
}
```

### A new factory

Declare it in `Factory.hpp`, define it in `Factory.cpp`. Start with `CreateE(world)`,
then fill components. Return the id.

### A new enemy type

Add an entry to `assets/data/enemies.json` with a new unique `id`. Spawn it with
`CreateEEnemy(world, pos, thatId)`. No C++ change is needed.

### A new attack or flight pattern

Add a `case` to `Fire` (attack) or `FlightVelocity` (flight) in `Behaviour.cpp`, then
use its number in `attack_pattern` / `flight_pattern`. For a bullet fan or ring,
call `Shoot(world, spawnerid, angle, speed)` for each bullet.

### A new enemy property

1. Add the member to `EnemyData` (with a default).
2. Read the key in `ReadEnemy` in `Loader.cpp`.
3. Use it in `CreateEEnemy`.
4. Document the key in section 6.

### A new source file

Add its `.cpp` to `add_executable` in `CMakeLists.txt`. Headers do not need listing.
A forgotten `.cpp` shows up as an `undefined reference` error at link time.

---

## 9. Pitfalls

These are mistakes that have already happened in this project.

- **`map[id]` creates the entry if it is missing.** Great for adding a component, wrong
  for checking one. Use `find` / `count` to look, `[]` to add. You will see the
  statement `world.Velocities[id];` in the factories: it is not a no-op, it creates
  the default (zero) velocity.
- **Do not erase from a map while looping over it** with a range-based `for`. It
  invalidates the loop. Collect the ids to remove, then call `Destroy` afterwards.
- **`Destroy` removes the entity, not references to it.** `EParent.ParentId` and
  `EOwnerId.OId` of other entities may still contain the dead id. Always `find`
  before using such an id.
- **Header and `.cpp` signatures must match exactly.** A different parameter list is a
  *different function* in C++: it compiles, then fails at link time when called.
- **Shadowing.** `float dt = ...;` inside a loop declares a *new* variable and leaves
  the outer `dt` untouched. Assign without the type: `dt = ...;`.
- **Rectangles are positioned by their top-left corner; our circles by their centre.**
- **Limited draw order.** `Draw` paints all sprites, then all rectangles, then all
  circles. Within a kind the order is arbitrary. `EZIndex` exists but nothing uses it.
- **Sprites and circles are centred on the position; the player's rectangle is not.**
- **Collision is brute force.** `Collision` compares every hitbox with every hurtbox.
  Fine for hundreds of bullets, too slow for thousands.
- **Keyboard.** `sf::Keyboard::isKeyPressed` reads the keyboard globally, even when the
  window is not focused. Keys are named by the letter they produce, which is why Z Q S D
  works on AZERTY.
- **SFML objects are not copyable** (windows) or heavy (textures, fonts). Do not store
  them in components.
- **Many bullets will be slow with this storage.** Each entity is an entry in about
  nine hash maps. It is fine for hundreds. For thousands, expect to reuse dead bullets
  instead of creating new ones, to store components in vectors, and to draw in
  batches.

---

## 10. What is not built yet

In rough order of what unlocks the most:

1. **A pattern definition format.** Patterns are `switch` cases in code. For many
   patterns, or to share them between games, describe them in data (JSON): bullet
   count, angles, speed, delays.
2. **Waves**: a script of "at time T, spawn enemy E at X" (also data). Today `main`
   spawns two enemies by hand, and `CreateEEnemyFollower` is never called.
3. **Game states**: pause, game over (the player is simply destroyed at 0 HP), a score.
   Global state such as "paused" is not a component.
4. **UI**: `EText`, `EButton`, with a shared font store like the texture store.
5. **A spatial index** (a quadtree) for collisions with thousands of bullets. It is
   *global* data living in `World`, not a component, rebuilt each frame.
6. **Animation**: use `EAnimation` to step through the cells of a sprite sheet.
7. **Draw order** with `EZIndex`, and batching bullets into one draw call.
8. **Audio**, and the RPG components' systems.
9. **Bullet reuse**: recycle dead bullets instead of creating new entities.
10. **A generic registry** (templates, one container per type found automatically),
    to remove the hand-written map and `erase` line per component. Worth doing
    when the component count makes the manual step error-prone.

---

## 11. Glossary

- **Entity**: an id (`int`) naming one thing in the game.
- **Component**: a data class attached to an entity.
- **System**: a function that processes entities having certain components.
- **World**: the container with every component map.
- **Factory**: a function that builds an entity with a given set of components.
- **Tag**: an empty component. Having it is the information.
- **Hitbox / hurtbox**: the area that deals damage / the area that takes damage.
- **Layer / mask**: bit flags saying what an entity is / what it can hit.
- **Pattern**: data describing how bullets are fired (attack) or how an enemy moves
  (flight).
- **`dt`**: delta time, the seconds since the previous frame.
- **Texture store**: `World::Textures`, where each image is loaded once and referred to by id.
- **Handle**: a small id standing for a shared resource (a texture, a font) so
  components do not hold the resource itself.
- **Bullet hell**: a shooter where the screen fills with bullets. The player's real
  hit area is tiny.
