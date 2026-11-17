# Architecture

## Overview

N-Type has a graphical client and an authoritative server. Both link the same
protocol and ECS libraries, but the server owns canonical game state.

```text
keyboard -> ClientNetwork -> UDP -> NetworkThread -> Server/Lobby/Game -> ECS
                ^                                                   |
                +------ lobby states and multipart snapshots <------+
                |
             ClientApp -> mirrored ECS -> SFML rendering and audio
```

Clients submit a five-bit input mask, never positions, damage, or scores. The
server applies input, simulates the world, and returns the result.

## Build targets

| Target | Responsibility |
| --- | --- |
| `ntype_protocol` | Packet construction and strict binary decoding |
| `ntype_shared` | Protocol, Asio, threads, constants, and CLI options |
| `ntype_engine` | ECS storage, factories, systems, combat, loaders |
| `ntype_server_core` | Testable server behavior without `main` |
| `r-type_server` | Server executable and signal handling |
| `ntype_client_core` | Window-independent client network session |
| `r-type_client` | Launcher, rendering, audio, HUD, and results |
| `ntype_sandbox` | Engine playground without networking |

## Server runtime

The server uses two threads:

1. `NetworkThread` owns the Asio UDP socket. It queues received datagrams for the
   game thread and schedules outgoing sends on its Asio context.
2. The main thread runs a fixed 60 Hz loop. It drains packets, handles console
   commands and timeouts, advances games, and schedules sends.

`Server` owns the client registry and `Lobbies`. Each `Lobby` owns membership,
leadership, roles, scores, phase, and an optional `Game`. Each active `Game` owns
an independent ECS `World`, so lobbies progress independently.

At every tick, the server sends a complete snapshot of each active game. Lobby
states are sent every six ticks (10 Hz). Silent clients time out after five seconds.
Limits are 128 connected clients, 32 lobbies, and four members per lobby.

## Client runtime

`client/src/main.cpp` retains one SFML window and one `ClientNetwork` session while
switching between `Launcher` and `ClientApp`.

`ClientNetwork` is UI-independent. It resolves the endpoint, connects, sends input
and heartbeats, repeats idempotent lobby requests, reassembles multipart snapshots,
and exposes state to the UI. Tests can therefore exercise networking without a window.

`ClientApp` mirrors the latest complete snapshot into a local ECS `World`. That
world is presentation-only: sprites, animation, hit flashes, explosions, parallax,
audio, HUD, and rendering. It never becomes authoritative.

## ECS model

- An **entity** is an integer ID.
- A **component** is data in a map keyed by entity ID.
- A **system** iterates matching component maps and applies behavior.

`World.hpp` contains definition tables, the next entity ID, and one map per
component type. `World::Destroy` removes an ID from every component map. IDs are
not reused inside a world.

| Source | Role |
| --- | --- |
| `System.cpp` | Frame orchestration, movement, animation, drawing, input |
| `Behaviour.cpp` | Flight patterns and projectile spawning |
| `Combat.cpp` | Collision, damage, invincibility, death, cleanup |
| `Items.cpp` | Pickup detection and effects |
| `Factory.cpp` | Recipes for players, enemies, shots, and items |
| `Loader.cpp` | JSON parsing and reference validation |

An enemy becomes a shooter through `ESpawner`, follows a pattern through `EFlight`,
receives damage through `EHurtbox`, and becomes visible through `ESprite`.

## Game-data flow

At startup, the server loads:

```text
attack_patterns.json ----+
flight_patterns.json ----+--> World definition tables
enemies.json ------------+
items.json --------------+
levels.json -----------------> timed waves referencing enemy IDs
```

Texture metadata comes from `textures.json`. IDs are stable links between files
and network snapshot `type` fields. Required malformed fields, unreadable files,
or unknown level enemies stop server startup. See the [Content Guide](CONTENT_GUIDE.md).

## Network model

Each UDP datagram contains one message. There is no generic header, sequence
number, acknowledgement, authentication, encryption, or version negotiation. The
first byte is its type; multibyte values are little-endian.

Reliability is purpose-specific:

- Connect repeats every 500 ms until answered.
- Lobby mutations repeat every 250 ms until state proves their effect.
- Lobby lists refresh every second.
- Full-state snapshots continuously replace earlier snapshots.
- `EntityDied` is best-effort; losing it only loses an explosion effect.
- Disconnect is sent three times; heartbeat timeout is the fallback.

Snapshots hold at most 90 entities per datagram and up to 32 parts. The client
publishes a frame only when all parts arrive, displaying its previous complete
frame meanwhile. See [Protocol v2](PROTOCOL_v2.md).

## Important invariants

- Player slots are 1–4 and are also their network entity IDs.
- Other network entity IDs begin at 5.
- Player positions are the top-left of a 40×40 box; other positions are centers.
- The logical viewport is 1280×720.
- Cross-file JSON IDs must exist and remain stable.
- Only the network thread touches the UDP socket.
- Only the server decides simulation, roles, score, and progression.

## Repository map

| Path | Purpose |
| --- | --- |
| `client/src/Launcher.cpp` | Title, connection, menus, lobby screens, settings |
| `client/src/ClientNetwork.*` | Session and snapshot reassembly |
| `client/src/ClientApp.*` | Gameplay presentation and snapshot mirroring |
| `server/src/Server.*` | Main loop, clients, routing, timeouts, sends |
| `server/src/Lobby.*` | Membership, roles, lifecycle, score, progression |
| `server/src/Game.*` | ECS simulation, waves, snapshots, game events |
| `server/src/NetworkThread.*` | Asio transport and queues |
| `common/protocol/` | Wire types, encoding, decoding |
| `engine/include/World.hpp` | ECS storage and destruction |
| `engine/include/Component.hpp` | Component definitions |
| `engine/src/` | Factories, systems, combat, items, loaders |
| `assets/data/` | Runtime content definitions |
| `tests/` | Unit, integration, end-to-end coverage |

## Known boundaries

The architecture omits client prediction, interpolation, lag compensation,
persistence, authentication, encryption, server migration, and wire-version
negotiation. Incompatible packet changes require coordinated client/server updates.
