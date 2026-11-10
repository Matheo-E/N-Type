# Content Guide

Most gameplay definitions live in `assets/data`. This guide explains how to add
content without C++ and when a code change is required.

## General rules

- Use a unique integer `id` in each JSON collection and avoid renumbering it.
- IDs link enemies, attacks, items, textures, levels, and network entity types.
- Texture paths are relative to `assets/` and use `/` separators.
- Positions/radii are pixels, time is seconds, and speed is pixels per second.
- Angles are degrees. Copy a nearby working pattern before tuning a new one.
- Start the server after each change. Loaders reject malformed JSON and several
  missing references, but do not validate every gameplay range.

## Textures and sprites

Place an image under `assets/sprites/`, then add it to `textures.json`:

```json
{ "id": 27, "path": "sprites/items/example.png" }
```

Reference it with a sprite block:

```json
{
  "texture": 27,
  "rect": [0, 0, 256, 256],
  "scale": 0.25,
  "flip_x": false,
  "animation": { "frames": [0, 1, 2, 1], "frame_time": 0.125 }
}
```

`rect` identifies one frame. Animation frames are offsets along the sheet.
`flip_x` and `animation` are optional.

## Attack patterns

Append to `attack_patterns.json`:

```json
{
  "id": 10,
  "name": "example_fan",
  "count": 5,
  "spread": 60,
  "aimed": true,
  "speed": 240,
  "damage": 1,
  "radius": 8,
  "lifetime": 8,
  "sprite": { "texture": 27, "rect": [0, 0, 256, 256], "scale": 0.1 }
}
```

| Field | Meaning | Default |
| --- | --- | ---: |
| `count` | Projectiles per volley | 1 |
| `spread` | Total angle covered | 0 |
| `angle` | Fixed direction when not aimed | 0 |
| `aimed` | Center volley on a player | false |
| `speed` | Projectile speed | 200 |
| `damage` | Base damage | 1 |
| `radius` | Hitbox radius | 5 |
| `lifetime` | Cleanup delay | 10 |
| `flight_pattern` | Optional flight ID | -1 |
| `sprite` | Optional appearance | none |

Pattern ID `0` is the player's default shot.

## Flight patterns

Supported `type` values are `stationary`, `straight`, `sine`, `chase`, and `path`:

```json
{
  "id": 4,
  "name": "wide_wave",
  "type": "sine",
  "direction": -90,
  "forward": 1,
  "amplitude": 140,
  "frequency": 0.25
}
```

`direction` is the base heading, `forward` scales forward motion, and sine
`amplitude`/`frequency` control sideways velocity. A path contains timed steps:

```json
{
  "id": 5,
  "name": "entrance_path",
  "type": "path",
  "steps": [
    { "move": [900, 220], "duration": 1.5 },
    { "wait": true, "duration": 0.5 },
    { "curve": [[850, 100], [650, 620], [500, 360]], "duration": 2.0 },
    { "arc": { "radius": 80, "start": 180, "sweep": 360 }, "duration": 2.5 }
  ]
}
```

Each step needs positive `duration` and exactly one of `move`, `wait`, `curve`,
or `arc`. Move and curve destinations use world coordinates.

## Enemies

Append to `enemies.json`:

```json
{
  "id": 13,
  "name": "example_enemy",
  "score": 300,
  "hp": 18,
  "contact_damage": 1,
  "radius": 24,
  "speed": 70,
  "fire_interval": 1.5,
  "attack_pattern": 10,
  "flight_pattern": 4,
  "sprite": { "texture": 27, "rect": [0, 0, 256, 256], "scale": 0.25 }
}
```

Use `-1` for no attack or flight pattern. `boss: true` is metadata; progression
is determined by the enemy referenced by a level's `boss`. `description` is for
documentation only. The server sends the enemy ID as snapshot `type`, so deploy
matching client assets and server data together.

## Items

Append to `items.json`:

```json
{
  "id": 2,
  "name": "repair",
  "radius": 18,
  "speed": 100,
  "lifetime": 12,
  "flight_pattern": 2,
  "sprite": { "texture": 27, "rect": [0, 0, 256, 256], "scale": 0.12 },
  "effects": { "heal": 1 }
}
```

Effects: `heal`, `max_hp`, `speed`, `fire_interval`, `shooting_type`,
`invincibility`, `attack`, and `defense`. They are additive except
`shooting_type`, which replaces the attack pattern. Fire interval cannot fall
below 0.05 seconds.

Item definitions, pickup behavior, rendering, and protocol support exist, but
current levels do not spawn drops. A JSON entry alone will not appear: a server
drop rule must call `CreateEItem`.

## Levels

Append to `levels.json`:

```json
{
  "id": 4,
  "name": "example",
  "waves": [
    { "time": 2, "enemy": 13, "count": 3, "interval": 1, "x": 1320, "y": 150, "y_step": 180 }
  ],
  "boss": { "time": 45, "enemy": 14, "x": 1100, "y": 360 }
}
```

| Field | Meaning | Default |
| --- | --- | ---: |
| `time` | Seconds after level start | required |
| `enemy` | ID from `enemies.json` | required |
| `count` | Number spawned | 1 |
| `interval` | Delay between spawns | 1 |
| `x`, `y` | First spawn position | 1320, 360 |
| `y_step` | Vertical offset per next enemy | 0 |

The boss always spawns once. Killing it advances to the next JSON level; killing
the final boss wins.

A level also needs client presentation:

1. Add `day/` and `night/` folders under `assets/backgrounds/<level-folder>/`.
2. Provide `layer_01_far.png` through `layer_04_ground.png` in each.
3. Add music under `assets/audio/music/`.
4. Add the folder/music to `kLevelFolders` and `kMusicFiles` in
   `client/src/BackgroundConfig.hpp` in the same order as JSON levels.

## Components and systems

For behavior that existing fields cannot express:

1. Define data-only state in `engine/include/Component.hpp`.
2. Add its map to `World` in `engine/include/World.hpp`.
3. Erase it in `World::Destroy` to prevent stale component data.
4. Attach it in `Factory.cpp` and implement behavior in a system file.
5. Call the system from the server step in the correct order.
6. Extend snapshots only when the client needs the state.
7. Add lifecycle and behavior tests.

Do not place authoritative gameplay in `ClientApp`; client state is cosmetic.

## Validation checklist

- JSON parses and all referenced IDs exist.
- Paths use exact Linux casing.
- Server starts and reports the expected levels.
- Content works through a real client/server session.
- CTest and the UDP smoke test pass.
- Protocol tests/docs change with wire-visible changes.
- Player-visible content updates its catalogue/documentation.
