# N-Type — Target Architecture Plan

This document describes what the **finished** N-Type project looks like: the deliverables,
the layers, the subsystems, the runtime flow and the repository layout. It is a plan, not
a description of the current state. Coding conventions live in `ARCHITECTURE.md`; the class
level view lives in `CLASS_UML.md`.

Subject requirements this plan satisfies (from `pdf/rtype-project.pdf`):

- Two binaries: `r-type_server` and `r-type_client`, C++, Linux mandatory, Windows target.
- Build system generator + package manager, self-contained build (no system libs).
- Authoritative, **multithreaded** server; graphical client; decoupled Rendering / Networking /
  Game Logic subsystems; a real (reusable) game engine.
- **Binary protocol over UDP**, robust to malformed packets, documented as an RFC.
- Timers (not CPU speed) drive time; star-field, 4 distinguishable players, Bydos, missiles.
- Part 2 tracks (Advanced Architecture / Networking / Gameplay) must fit without redesign.

---

## 1. Technology decisions

| Concern              | Choice                                   | Why (short) |
|----------------------|------------------------------------------|-------------|
| Language / standard  | C++20                                    | concepts, `std::span`, designated initialisers, `<bit>` for endianness |
| Build                | CMake ≥ 3.25, presets (`CMakePresets.json`) | de-facto standard, Windows + Linux, IDE support |
| Package manager      | vcpkg (manifest mode, `vcpkg.json`)      | binary caching keeps CI cheap, first-class MSVC support. CPM is the fallback if vcpkg is a problem the machines |
| Rendering/Audio/Input| SFML 2.6                                 | allowed by subject, simple 2D API, cross-platform |
| Networking           | standalone Asio                          | allowed by subject, async UDP, portable, no Boost |
| Serialization        | hand-written binary codec (`ByteBuffer`) | protocol MUST be binary and documented; no dependency needed |
| Config / levels      | JSON (nlohmann-json) in Part 1, Lua (sol2) scripting in Part 2 | data-driven content without recompiling |
| Tests                | GoogleTest via CTest                     | engine/protocol are headless and unit-testable |
| Tooling              | clang-format, clang-tidy, GitHub Actions (Linux + Windows matrix) | required "well-defined workflow" |
| Docs                 | Markdown + MkDocs (static site), Doxygen for API | "modern, navigable" documentation requirement |

Dependency rule: **the engine never includes SFML or Asio headers in its public API**.
Backends are behind small interfaces so the server links the engine without any graphics.

---

## 2. High-level layering

Dependencies flow strictly downward. A lower box never knows about a higher one.

```
┌──────────────────────────────────────────────────────────────────────────┐
│  APPLICATIONS (interfaces/ layer)                                        │
│   r-type_client            r-type_server            (Part 2) 2nd game    │
│   window, states, HUD,     lobby, game instances,   e.g. pong / other    │
│   prediction/interp        authoritative sim                             │
├──────────────────────────────────────────────────────────────────────────┤
│  GAME: rtype_common (services/ layer, shared by client & server)         │
│   R-Type components, systems, prefabs/factories, wave & level data,      │
│   protocol messages specific to R-Type (spawn enemy type X ...)          │
├──────────────────────────────────────────────────────────────────────────┤
│  ENGINE (core/ layer) — game-agnostic, standalone library "ntype-engine" │
│   ecs        events      time/loop   resources   scene/state   math      │
│   net (UDP transport, packets, reliability)   platform interfaces        │
│   backends: sfml (render/audio/input)  — optional, compile-time module   │
├──────────────────────────────────────────────────────────────────────────┤
│  THIRD PARTY:  SFML   Asio   nlohmann-json   (sol2/Lua Part 2)   gtest   │
└──────────────────────────────────────────────────────────────────────────┘
```

Namespaces mirror this: `ntype::engine::ecs`, `ntype::engine::net`, `ntype::engine::render`,
`ntype::rtype`, `ntype::server`, `ntype::client`.

---

## 3. Engine subsystems (library `ntype-engine`)

### 3.1 ECS (`engine/ecs`)
- `Entity`: 32-bit handle = index + generation (safe against stale handles).
- `Registry`: owns all `ComponentStorage<T>` (sparse-set per component type), creates/destroys
  entities, provides `view<Ts...>()` iteration.
- `ISystem` + `SystemScheduler`: systems are ordered, each gets `(Registry&, float dt)`.
  The same scheduler runs on the server (simulation systems) and the client (render systems).
- Components are plain aggregates (POD-ish structs). No logic in components.

Why ECS: the subject explicitly asks for decoupling (renderer only needs Transform+Sprite),
and the same component data is what we serialize to the network.

### 3.2 Events / message passing (`engine/events`)
- `EventBus`: typed publish/subscribe (Mediator pattern). Systems never call each other
  directly; e.g. `CollisionSystem` publishes `CollisionEvent`, `DamageSystem` consumes it.
- Queued delivery: events raised during a tick are dispatched at a defined point, so
  ordering is deterministic (needed for an authoritative server).

### 3.3 Time (`engine/time`)
- `Clock` (steady_clock wrapper) and `GameLoop` with **fixed timestep** simulation
  (server: 60 Hz tick) and variable render step (client). Guarantees time flow is independent
  of CPU speed as required.

### 3.4 Resources (`engine/resources`)
- `ResourceManager<T>`: id → cached `T` (textures, sound buffers, fonts, JSON docs). Loads
  on first access, supports preloading manifests. Game code refers to assets by string id.

### 3.5 Scene / state (`engine/scene`)
- `StateMachine` + `IState` (enter/exit/handle_input/update/render). Client states:
  Menu → Lobby → Play → GameOver. Server "states" are per game instance (Waiting → Running →
  Finished).

### 3.6 Platform interfaces & SFML backend (`engine/platform`, `engine/backends/sfml`)
- `IWindow`, `IRenderer`, `IInputProvider`, `IAudio`. Exactly one real implementation in
  Part 1 (SFML) plus a `NullRenderer`/`NullAudio` used by tests and by the server build —
  that is the "two implementations" that justify the abstraction.
- SFML types never leak into engine or game headers.

### 3.7 Networking (`engine/net`)
- `UdpSocket`: RAII wrapper around an Asio UDP socket (open/bind/send_to/async_receive).
- `ByteBuffer` + `Serializer`/`Deserializer`: little-endian, fixed-width integer encoding,
  bounds-checked reads → a truncated or malformed packet yields an error, never UB.
- `PacketHeader` { magic, version, type, sequence, ack, ack_bits, payload_size } — common to
  every datagram; decoder validates magic/version/size before anything else.
- `ReliableChannel`: sequence numbers, ack bitfield, retransmit of messages flagged
  reliable (connect, disconnect, player died, game over). Everything else is fire-and-forget.
- `Connection`: remote endpoint, last-heard timestamp, channel state, RTT estimate.
  Timeouts detect crashed clients (server MUST survive and notify others).
- `NetworkThread`: runs the Asio `io_context` on its own thread, pushes decoded messages
  into a `ThreadSafeQueue<InboundMessage>` consumed by the game thread; outbound goes the
  other way. **This is the server multithreading boundary**.

### 3.8 Math & utilities (`engine/math`, `engine/util`)
- `Vec2`, `Rect`, AABB helpers, `Random` (seeded so the server is reproducible), logging.

---

## 4. Shared game layer (`rtype_common`)

Compiled into both binaries so client and server agree on data.

- **Components**: `Transform`, `Velocity`, `Sprite`, `Animation`, `Collider`, `Health`,
  `Lifetime`, `Weapon`, `Team`, `NetworkId`, `PlayerInput`, `Score`, `PowerUp`, `EnemyBehavior`.
- **Simulation systems** (server-authoritative, client may run some for prediction):
  `MovementSystem`, `WeaponSystem`, `CollisionSystem`, `DamageSystem`, `LifetimeSystem`,
  `EnemyBehaviorSystem`, `SpawnSystem` (wave director), `BoundsSystem`.
- **Prefabs / factories**: `EntityFactory::create_player(idx)`, `create_bydos(type)`,
  `create_missile(owner)`, `create_powerup(...)`. Data comes from `assets/data/*.json`
  (sprite rects, speeds, HP, patterns) so adding a monster = adding a JSON entry.
- **Protocol** (`rtype_common/protocol`): the R-Type message set on top of the engine packet
  layer. Every message has a `MessageType` enum value, a fixed layout and a codec function.
  Documented in `docs/PROTOCOL.md` (RFC style) so a third party could write a client.

Message families:

| Direction | Messages |
|-----------|----------|
| C → S     | `ConnectRequest`, `Disconnect`, `Input` (bitmask + client tick), `Ping`, `JoinRoom`, `Ready` |
| S → C     | `ConnectAccept` (player id, seed, tick rate), `ConnectRefuse`, `EntitySpawn`, `EntityDestroy`, `WorldSnapshot` (delta of Transform/Velocity/Health for changed entities), `PlayerEvent` (died, scored), `ClientLeft` (crash/disconnect notification), `GameStateChange`, `Pong`, `RoomList` |

---

## 5. Server (`r-type_server`)

```
main thread                       network thread
─────────────────────────────     ───────────────────────────
ServerApp                         NetworkThread (Asio io_context)
 └─ RoomManager                    ├─ UdpSocket.async_receive → decode → inbound queue
     └─ GameInstance[*]            └─ outbound queue → encode → send_to
         ├─ Registry + Scheduler
         ├─ ClientSession[≤4]
         ├─ SnapshotBuilder
         └─ fixed 60 Hz GameLoop
```

- `ServerApp`: parses CLI/config (port, tick rate, max rooms), starts `NetworkThread`,
  drives `RoomManager` on the main thread. Catches every exception at the tick boundary and
  logs it; a bad packet or a misbehaving client never takes the process down.
- `RoomManager`: maps endpoints/sessions to `GameInstance`s. Part 1 = a single auto-created
  room; Part 2 = many rooms, lobby listing, per-room rules (Advanced Networking track). Each
  `GameInstance` can later be moved to its own worker thread without API change.
- `GameInstance`: authoritative simulation. Per tick: drain inputs → apply to `PlayerInput`
  components → run systems → collect `EventBus` events (spawn/destroy/death) into reliable
  messages → `SnapshotBuilder` produces the unreliable `WorldSnapshot` → broadcast.
- `ClientSession`: `Connection` + player entity + last processed input tick + ready flag.
- Timeout handling: session not heard for N seconds → destroy player entity, broadcast
  `ClientLeft`, keep the game running.

---

## 6. Client (`r-type_client`)

```
ClientApp
 ├─ SfmlWindow / SfmlRenderer / SfmlAudio / SfmlInput
 ├─ ResourceManager<Texture/SoundBuffer/Font>
 ├─ ClientNetwork (Asio on its own thread, same queue pattern as server)
 ├─ Settings (key bindings, colour-blind palette, volume, text scale — accessibility)
 └─ StateMachine
     ├─ MenuState      (connect dialog, settings)
     ├─ LobbyState     (room list / ready, Part 2)
     ├─ PlayState      (the game)
     │    ├─ Registry + Scheduler (render-side systems)
     │    ├─ WorldSync        applies EntitySpawn/Destroy/Snapshot → components
     │    ├─ InterpolationSystem  smooths remote entities between snapshots
     │    ├─ PredictionSystem     local ship moves immediately, reconciled on snapshot
     │    ├─ StarfieldSystem / ParallaxSystem
     │    ├─ AnimationSystem, RenderSystem, AudioSystem
     │    └─ Hud (score, lives, lagometer/FPS debug overlay)
     └─ GameOverState
```

- Input is sampled every frame, packed into an `Input` bitmask and sent every tick with the
  client tick number; server echoes last processed tick in snapshots for reconciliation.
- The client never decides gameplay outcomes: kills, pickups and deaths come only from the
  server.
- Accessibility (graded at both defenses): remappable keys, gamepad, colour-blind-safe
  player colours + shape/sprite differences, subtitles/visual cue for audio events,
  adjustable game speed/difficulty in coop, high-contrast UI, no flashing over 3 Hz.

---

## 7. Runtime flow (one tick)

1. Client: poll SFML events → `InputMapper` → `Input` message → outbound queue.
2. Network threads move datagrams both ways; decoders drop anything with bad
   magic/version/length (counted in metrics, never thrown across the thread boundary).
3. Server tick: drain inbound → per-session `PlayerInput` → `SystemScheduler::update(dt)`
   → `EventBus` flush → reliable events + `WorldSnapshot` → outbound.
4. Client frame: drain inbound → `WorldSync` → prediction/interpolation → render systems →
   `IRenderer::present()`.

---

## 8. Repository layout (final)

```
N-Type/
├── CMakeLists.txt            # top-level: options NTYPE_BUILD_CLIENT/SERVER/TESTS/SFML_BACKEND
├── CMakePresets.json
├── vcpkg.json
├── .clang-format  .clang-tidy
├── .github/workflows/ci.yml  # linux+windows build, ctest, format check, cached deps
├── engine/                   # standalone library, no R-Type knowledge
│   ├── include/ntype/engine/{ecs,events,time,resources,scene,math,net,platform}/
│   ├── src/...
│   └── backends/sfml/
├── rtype_common/             # shared game layer (components, systems, protocol, factories)
├── server/                   # r-type_server
├── client/                   # r-type_client
├── games/                    # Part 2: second sample game proving engine reuse
├── assets/
│   ├── sprites/  (r-typesheet*.gif → converted to png)
│   ├── audio/
│   ├── fonts/
│   └── data/     (enemies.json, waves.json, levels/*.json, later *.lua)
├── tests/                    # mirrors engine/ and rtype_common/ (gtest)
│   ├── engine/{ecs,net,events,...}
│   └── rtype_common/protocol/
├── tools/                    # Part 2: level editor, netem test scripts, packet inspector
├── docs/
│   ├── ARCHITECTURE.md       # coding guide (exists)
│   ├── ARCHITECTURE_PLAN.md  # this file
│   ├── CLASS_UML.md
│   ├── PROTOCOL.md           # RFC-style binary protocol spec
│   ├── COMPARATIVE_STUDY.md  # tech choices, storage, security
│   ├── ACCESSIBILITY.md
│   ├── CONTRIBUTING.md
│   └── tutorials/            # "add a monster", "add a level", "write a new client"
├── pdf/                      # subject
└── README.md
```

---

## 9. Threading model summary

| Thread              | Owns                                   | Communicates via |
|---------------------|----------------------------------------|------------------|
| server main         | RoomManager, GameInstances, tick loop  | inbound/outbound `ThreadSafeQueue` |
| server network      | Asio io_context, UdpSocket             | same queues |
| (Part 2) room worker| one GameInstance each                  | same queues, room-scoped |
| client main         | window, states, registry, rendering    | queues |
| client network      | Asio io_context, UdpSocket             | queues |

Only the queues are shared; no game state is touched from a network thread. This keeps the
concurrency surface tiny and testable.

---

## 10. Extensibility hooks for Part 2

- Advanced Architecture: engine already a separate CMake target → publish it as its own repo /
  package; add `games/<second-game>`; plugin API = `IModule` loaded from shared objects;
  Lua scripting for `EnemyBehavior`; dev console with CVars over the `EventBus`.
- Advanced Networking: `RoomManager` → multiple instances/threads, lobby/chat messages;
  `SnapshotBuilder` → delta compression + quantization; `ReliableChannel` already provides
  acks; prediction/interpolation systems exist as separate systems to be tuned; metrics
  overlay + netem scripts in `tools/` for measurements.
- Advanced Gameplay: content is JSON/Lua data + factories, so bosses, snake enemies, Force
  weapon and new levels are data + one behavior system each; level editor tool in `tools/`.

---

## 11. Delivery milestones

1. **Week 1** — skeleton: CMake/vcpkg/CI, engine ECS + EventBus + GameLoop, ByteBuffer codec,
   UdpSocket, unit tests, window with scrolling star-field.
2. **Week 2** — connect flow, player ships moving via server, snapshots, missiles, Bydos
   spawn waves, collisions/death on server, client interpolation, sounds.
3. **Week 3** — robustness (malformed packets, timeouts, crash notification), 4 distinct
   players, HUD, accessibility settings, PROTOCOL.md, README, docs site → **Defense 1**.
4. **Weeks 4–6** — chosen Part 2 tracks (see §10) → **Final defense**.
