# Draw layers

Which sprite is painted on top of which.

## The rule

Every entity that has an `ESprite` is drawn. When two overlap, the one painted
**later** is on top. `Draw` (in `engine/src/System.cpp`) decides the order like this:

1. Sort by the entity's **Z** (its `EZIndex`): the **lower Z is drawn first**, so it
   ends up behind. The higher Z is drawn last, on top.
2. If two entities have the same Z, the **lower entity id** is drawn first. A newer
   entity (for example a freshly fired bullet) lands above an older one.

An entity without an `EZIndex` counts as Z = 0.

## The layers

The numbers are constants in `engine/include/Component.hpp`. Only their order
matters. They have gaps so a new layer can be slotted in later without renumbering.

| Constant | Z | What lives there |
|---|---|---|
| `ZBackground` | -10 | scrolling backgrounds, decoration |
| `ZEnemy` | 0 | enemies, bosses |
| `ZBullet` | 1 | all bullets, the player's and the enemies' |
| `ZPlayer` | 2 | the player's ship |
| `ZEffect` | 3 | explosions and hit flashes |
| `ZUi` | 10 | score, lives, pause menu: always on top |

Why this order: in a bullet hell the player must always see their own ship and the
bullets that can hurt them. So decoration and enemies sit behind, bullets above them,
and the ship above the bullets.

## How to set it

The Z is chosen in the factory, when the entity is created. It is not computed. In
`engine/src/Factory.cpp`, the helper `AddSprite` takes the layer:

```cpp
AddSprite(world, id, Look(TexturePlayers, {67, 2, 32, 15}, 2.0f), ZPlayer);
```

It sets both the `ESprite` and the `EZIndex`, so an entity cannot be given a sprite and
forget its layer. A new kind of entity should pick the constant that fits, or add a new
constant here and in `Component.hpp`.

Currently set by the factories:

| Factory | Layer |
|---|---|
| `CreateEPlayer` | `ZPlayer` |
| `CreateEEnemy` (and followers) | `ZEnemy` |
| `CreateEBullet` | `ZBullet` |
| `CreateEStatic` | none: it has no sprite, so it is not drawn |

## Good to know

- **Entities without an `ESprite` are never drawn**, whatever their Z.
- **Two sprites on the same layer overlap by id.** If that looks wrong for some pair
  (say, a boss that should always be behind its own bullets), give them different layers.
- **Layers are not collision layers.** Collision uses `ELayer` (what an entity is and what
  it can hit). Drawing uses `EZIndex`. They are independent.
