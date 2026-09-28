# N-Type — Class UML

Class-level view of the target architecture described in `ARCHITECTURE_PLAN.md`.
Diagrams are Mermaid `classDiagram` blocks (rendered by GitHub, MkDocs-material, VS Code).
Split per subsystem to stay readable; the last section shows how the pieces connect.

Legend: `<<interface>>` = pure virtual class, `<<struct>>` = plain data (component/message),
`<<template>>` = class template. Naming follows `ARCHITECTURE.md`
(`PascalCase` types, `snake_case` members).

---

## 1. Engine — ECS core (`ntype::engine::ecs`)

```mermaid
classDiagram
    direction LR

    class Entity {
        <<struct>>
        +uint32_t id
        +index() uint32_t
        +generation() uint32_t
        +is_null() bool
    }

    class IComponentStorage {
        <<interface>>
        +remove(Entity) void
        +contains(Entity) bool
        +clear() void
    }

    class ComponentStorage~T~ {
        <<template>>
        -vector~uint32_t~ sparse
        -vector~Entity~ dense
        -vector~T~ components
        +emplace(Entity, T) T&
        +get(Entity) T&
        +try_get(Entity) T*
        +remove(Entity) void
        +each() range
    }

    class Registry {
        -vector~uint32_t~ generations
        -vector~uint32_t~ free_list
        -unordered_map~type_index, unique_ptr~IComponentStorage~~ storages
        +create() Entity
        +destroy(Entity) void
        +is_alive(Entity) bool
        +emplace~T~(Entity, T) T&
        +get~T~(Entity) T&
        +try_get~T~(Entity) T*
        +remove~T~(Entity) void
        +view~Ts...~() View~Ts...~
        +clear() void
    }

    class View~Ts...~ {
        <<template>>
        +each(callable) void
        +begin() iterator
        +end() iterator
    }

    class ISystem {
        <<interface>>
        +update(Registry&, float dt) void
        +name() string_view
    }

    class SystemScheduler {
        -vector~unique_ptr~ISystem~~ systems
        +add~S~(args...) S&
        +update(Registry&, float dt) void
    }

    IComponentStorage <|-- ComponentStorage
    Registry "1" *-- "*" IComponentStorage : owns
    Registry ..> Entity : creates
    Registry ..> View : produces
    SystemScheduler "1" *-- "*" ISystem : ordered
    ISystem ..> Registry : operates on
```

---

## 2. Engine — events, time, resources, scene

```mermaid
classDiagram
    direction TB

    class EventBus {
        -unordered_map~type_index, vector~Handler~~ handlers
        -vector~function~void()~~ queued
        +subscribe~E~(function~void(const E&)~) Subscription
        +publish~E~(E) void
        +enqueue~E~(E) void
        +dispatch_queued() void
    }
    class Subscription {
        +~Subscription()  unsubscribes
    }
    EventBus ..> Subscription : returns

    class Clock {
        -steady_clock::time_point last
        +restart() Duration
        +elapsed() Duration
    }
    class GameLoop {
        -Duration fixed_step
        -Duration accumulator
        -bool running
        +run(on_fixed_update, on_render) void
        +stop() void
        +ticks() uint64_t
    }
    GameLoop --> Clock

    class ResourceManager~T~ {
        <<template>>
        -unordered_map~string, shared_ptr~T~~ cache
        -function~unique_ptr~T~(path)~ loader
        +load(string id, path) T&
        +get(string id) T&
        +contains(string id) bool
        +unload(string id) void
    }

    class IState {
        <<interface>>
        +on_enter() void
        +on_exit() void
        +handle_event(const InputEvent&) void
        +update(float dt) void
        +render(IRenderer&) void
    }
    class StateMachine {
        -vector~unique_ptr~IState~~ stack
        +push(unique_ptr~IState~) void
        +pop() void
        +replace(unique_ptr~IState~) void
        +current() IState&
        +update(float dt) void
        +render(IRenderer&) void
    }
    StateMachine "1" *-- "*" IState
```

---

## 3. Engine — platform interfaces and SFML backend

```mermaid
classDiagram
    direction LR

    class IWindow {
        <<interface>>
        +poll_events() vector~InputEvent~
        +is_open() bool
        +close() void
        +size() Vec2u
    }
    class IRenderer {
        <<interface>>
        +begin_frame(Color clear) void
        +draw_sprite(TextureId, Rect src, Transform, Color tint) void
        +draw_rect(Rect, Color) void
        +draw_text(FontId, string_view, Vec2f, unsigned size, Color) void
        +end_frame() void
    }
    class IAudio {
        <<interface>>
        +play_sound(SoundId, float volume) void
        +play_music(MusicId, bool loop) void
        +stop_music() void
        +set_master_volume(float) void
    }
    class IInputProvider {
        <<interface>>
        +is_key_down(Key) bool
        +is_button_down(GamepadButton) bool
        +axis(GamepadAxis) float
    }

    class SfmlWindow
    class SfmlRenderer {
        -sf::RenderWindow& target
        -ResourceManager~sf::Texture~& textures
    }
    class SfmlAudio {
        -vector~sf::Sound~ pool
        -sf::Music music
    }
    class SfmlInput
    class NullRenderer
    class NullAudio

    IWindow <|.. SfmlWindow
    IRenderer <|.. SfmlRenderer
    IRenderer <|.. NullRenderer
    IAudio <|.. SfmlAudio
    IAudio <|.. NullAudio
    IInputProvider <|.. SfmlInput
    SfmlRenderer --> SfmlWindow : draws into
```

---

## 4. Engine — networking (`ntype::engine::net`)

```mermaid
classDiagram
    direction TB

    class ByteBuffer {
        -vector~byte~ data
        -size_t read_pos
        +write~T~(T) void
        +read~T~() expected~T, DecodeError~
        +write_bytes(span~const byte~) void
        +remaining() size_t
        +bytes() span~const byte~
    }

    class PacketHeader {
        <<struct>>
        +uint16_t magic
        +uint8_t  version
        +uint8_t  type
        +uint16_t sequence
        +uint16_t ack
        +uint32_t ack_bits
        +uint16_t payload_size
        +kSize$ size_t
    }

    class Packet {
        <<struct>>
        +PacketHeader header
        +vector~byte~ payload
        +encode() ByteBuffer
        +decode(span~const byte~)$ expected~Packet, DecodeError~
    }

    class DecodeError {
        <<enumeration>>
        BadMagic
        BadVersion
        Truncated
        OversizedPayload
        UnknownType
    }

    class UdpSocket {
        -asio::ip::udp::socket socket
        +UdpSocket(io_context&, uint16_t port)
        +send_to(span~const byte~, Endpoint) void
        +async_receive(callback) void
        +local_endpoint() Endpoint
    }

    class ReliableChannel {
        -uint16_t local_sequence
        -uint16_t remote_sequence
        -uint32_t remote_ack_bits
        -deque~PendingPacket~ unacked
        +next_sequence() uint16_t
        +on_received(PacketHeader) void
        +on_acked(uint16_t seq) void
        +packets_to_resend(TimePoint now) vector~Packet~
        +is_duplicate(uint16_t seq) bool
    }

    class Connection {
        +Endpoint remote
        +TimePoint last_heard
        +ReliableChannel channel
        +float rtt_ms
        +touch(TimePoint) void
        +timed_out(TimePoint, Duration) bool
    }

    class ThreadSafeQueue~T~ {
        <<template>>
        -mutex m
        -deque~T~ items
        +push(T) void
        +try_pop() optional~T~
        +drain() vector~T~
    }

    class InboundMessage {
        <<struct>>
        +Endpoint from
        +Packet packet
        +TimePoint received_at
    }
    class OutboundMessage {
        <<struct>>
        +Endpoint to
        +Packet packet
        +bool reliable
    }

    class NetworkThread {
        -asio::io_context io
        -UdpSocket socket
        -jthread worker
        -ThreadSafeQueue~InboundMessage~& inbound
        -ThreadSafeQueue~OutboundMessage~& outbound
        -NetStats stats
        +start() void
        +stop() void
        +stats() NetStats
    }

    class NetStats {
        <<struct>>
        +uint64_t packets_in
        +uint64_t packets_out
        +uint64_t bytes_in
        +uint64_t bytes_out
        +uint64_t dropped_malformed
    }

    Packet *-- PacketHeader
    Packet ..> ByteBuffer
    Packet ..> DecodeError
    Connection *-- ReliableChannel
    NetworkThread *-- UdpSocket
    NetworkThread --> ThreadSafeQueue : inbound / outbound
    NetworkThread *-- NetStats
    ThreadSafeQueue ..> InboundMessage
    ThreadSafeQueue ..> OutboundMessage
```

---

## 5. Shared game layer — components (`ntype::rtype`)

All components are `<<struct>>` aggregates; no methods beyond trivial helpers.

```mermaid
classDiagram
    direction LR

    class Transform {
        +Vec2f position
        +float rotation
        +Vec2f scale
    }
    class Velocity {
        +Vec2f linear
    }
    class Sprite {
        +TextureId texture
        +Rect source
        +int layer
        +Color tint
    }
    class Animation {
        +vector~Rect~ frames
        +float frame_time
        +float elapsed
        +size_t current
        +bool loop
    }
    class Collider {
        +Rect bounds
        +CollisionLayer layer
        +CollisionMask mask
    }
    class Health {
        +int current
        +int max
    }
    class Lifetime {
        +float remaining
    }
    class Weapon {
        +float cooldown
        +float since_last_shot
        +ProjectileType projectile
        +bool trigger_held
    }
    class Team {
        +TeamId id
    }
    class NetworkId {
        +uint32_t id
        +bool dirty
    }
    class PlayerInput {
        +InputBits bits
        +uint32_t client_tick
    }
    class Score {
        +uint32_t points
    }
    class EnemyBehavior {
        +BehaviorType type
        +float phase
        +Vec2f origin
    }
    class PowerUp {
        +PowerUpType type
    }
    class Player {
        +uint8_t slot
        +uint8_t lives
    }
```

---

## 6. Shared game layer — systems, factory, protocol

```mermaid
classDiagram
    direction TB

    class ISystem {
        <<interface>>
        +update(Registry&, float dt) void
    }

    class MovementSystem
    class BoundsSystem {
        -Rect world
    }
    class WeaponSystem {
        -EntityFactory& factory
        -EventBus& events
    }
    class CollisionSystem {
        -EventBus& events
    }
    class DamageSystem {
        -EventBus& events
        -Subscription on_collision
    }
    class LifetimeSystem
    class EnemyBehaviorSystem
    class SpawnSystem {
        -WaveScript waves
        -EntityFactory& factory
        -Random rng
        -float elapsed
    }

    ISystem <|.. MovementSystem
    ISystem <|.. BoundsSystem
    ISystem <|.. WeaponSystem
    ISystem <|.. CollisionSystem
    ISystem <|.. DamageSystem
    ISystem <|.. LifetimeSystem
    ISystem <|.. EnemyBehaviorSystem
    ISystem <|.. SpawnSystem

    class EntityFactory {
        -Registry& registry
        -const GameData& data
        -uint32_t next_network_id
        +create_player(uint8_t slot) Entity
        +create_bydos(EnemyTypeId, Vec2f) Entity
        +create_missile(Entity owner, ProjectileType) Entity
        +create_powerup(PowerUpType, Vec2f) Entity
        +create_obstacle(Rect) Entity
    }

    class GameData {
        +unordered_map~EnemyTypeId, EnemyDef~ enemies
        +unordered_map~ProjectileType, ProjectileDef~ projectiles
        +vector~WaveDef~ waves
        +load_from_json(path)$ GameData
    }
    class EnemyDef {
        <<struct>>
        +string sprite
        +int hp
        +float speed
        +BehaviorType behavior
        +optional~PowerUpType~ drop
    }
    class WaveDef {
        <<struct>>
        +float at_time
        +EnemyTypeId type
        +uint8_t count
        +float spacing
    }

    GameData *-- EnemyDef
    GameData *-- WaveDef
    EntityFactory --> GameData
    SpawnSystem --> EntityFactory
    WeaponSystem --> EntityFactory

    class MessageType {
        <<enumeration>>
        ConnectRequest
        ConnectAccept
        ConnectRefuse
        Disconnect
        Input
        Ping
        Pong
        EntitySpawn
        EntityDestroy
        WorldSnapshot
        PlayerEvent
        ClientLeft
        GameStateChange
        JoinRoom
        RoomList
    }

    class Codec {
        <<utility>>
        +encode(const ConnectRequest&)$ Packet
        +encode(const Input&)$ Packet
        +encode(const WorldSnapshot&)$ Packet
        +decode(const Packet&)$ expected~Message, DecodeError~
    }
    class Message {
        <<variant>>
        ConnectRequest | ConnectAccept | Input | EntitySpawn | EntityDestroy | WorldSnapshot | PlayerEvent | ClientLeft | ...
    }
    class WorldSnapshot {
        <<struct>>
        +uint32_t server_tick
        +uint32_t last_processed_input
        +vector~EntityState~ entities
    }
    class EntityState {
        <<struct>>
        +uint32_t network_id
        +int16_t x, y
        +int16_t vx, vy
        +uint8_t hp
        +uint8_t flags
    }
    class EntitySpawn {
        <<struct>>
        +uint32_t network_id
        +EntityKind kind
        +uint8_t subtype
        +int16_t x, y
    }

    Codec ..> MessageType
    Codec ..> Message
    WorldSnapshot *-- EntityState
```

---

## 7. Server (`ntype::server`)

```mermaid
classDiagram
    direction TB

    class ServerApp {
        -ServerConfig config
        -ThreadSafeQueue~InboundMessage~ inbound
        -ThreadSafeQueue~OutboundMessage~ outbound
        -NetworkThread network
        -RoomManager rooms
        -GameLoop loop
        +ServerApp(ServerConfig)
        +run() int
        +request_stop() void
    }
    class ServerConfig {
        <<struct>>
        +uint16_t port
        +unsigned tick_rate
        +unsigned max_rooms
        +Duration client_timeout
        +from_args(argc, argv)$ ServerConfig
    }

    class RoomManager {
        -vector~unique_ptr~GameInstance~~ instances
        -unordered_map~Endpoint, GameInstance*~ routing
        -EntityFactory factory
        +handle(InboundMessage) void
        +tick(float dt, TimePoint now) void
        +find_or_create_room() GameInstance&
        +room_list() vector~RoomInfo~
    }

    class GameInstance {
        -RoomId id
        -InstancePhase phase
        -Registry registry
        -SystemScheduler scheduler
        -EventBus events
        -EntityFactory factory
        -array~optional~ClientSession~, 4~ sessions
        -SnapshotBuilder snapshots
        -ThreadSafeQueue~OutboundMessage~& outbound
        -uint32_t tick
        +join(Endpoint) expected~uint8_t, JoinError~
        +leave(Endpoint, LeaveReason) void
        +handle_input(Endpoint, Input) void
        +tick_once(float dt, TimePoint now) void
        -check_timeouts(TimePoint) void
        -broadcast(Packet, bool reliable) void
    }
    class InstancePhase {
        <<enumeration>>
        Waiting
        Running
        Finished
    }

    class ClientSession {
        +Connection connection
        +uint8_t slot
        +Entity player
        +uint32_t last_input_tick
        +bool ready
    }

    class SnapshotBuilder {
        -unordered_map~uint32_t, EntityState~ last_sent
        +build(const Registry&, uint32_t tick, uint32_t last_input) WorldSnapshot
        +spawn_message(const Registry&, Entity) EntitySpawn
    }

    ServerApp *-- ServerConfig
    ServerApp *-- NetworkThread
    ServerApp *-- RoomManager
    ServerApp *-- GameLoop
    RoomManager "1" *-- "0..*" GameInstance
    GameInstance *-- Registry
    GameInstance *-- SystemScheduler
    GameInstance *-- EventBus
    GameInstance *-- EntityFactory
    GameInstance "1" *-- "0..4" ClientSession
    GameInstance *-- SnapshotBuilder
    GameInstance --> InstancePhase
    ClientSession *-- Connection
```

---

## 8. Client (`ntype::client`)

```mermaid
classDiagram
    direction TB

    class ClientApp {
        -ClientConfig config
        -Settings settings
        -SfmlWindow window
        -SfmlRenderer renderer
        -SfmlAudio audio
        -SfmlInput input
        -ResourceManager~sf::Texture~ textures
        -ResourceManager~sf::SoundBuffer~ sounds
        -ResourceManager~sf::Font~ fonts
        -ClientNetwork network
        -StateMachine states
        -GameLoop loop
        +run() int
    }

    class Settings {
        +KeyBindings bindings
        +ColorPalette palette
        +float master_volume
        +float text_scale
        +bool high_contrast
        +bool reduce_motion
        +load(path)$ Settings
        +save(path) void
    }
    class KeyBindings {
        +unordered_map~Action, Key~ keyboard
        +unordered_map~Action, GamepadButton~ gamepad
    }
    class InputMapper {
        -const KeyBindings& bindings
        +sample(const IInputProvider&) InputBits
    }

    class ClientNetwork {
        -ThreadSafeQueue~InboundMessage~ inbound
        -ThreadSafeQueue~OutboundMessage~ outbound
        -NetworkThread thread
        -Connection server
        +connect(Endpoint) void
        +send(Message, bool reliable) void
        +poll() vector~Message~
        +rtt_ms() float
        +is_connected() bool
    }

    class IState {
        <<interface>>
    }
    class MenuState {
        -TextField address
        -SettingsPanel settings_ui
    }
    class LobbyState {
        -vector~RoomInfo~ rooms
    }
    class PlayState {
        -Registry registry
        -SystemScheduler scheduler
        -EventBus events
        -WorldSync sync
        -Hud hud
        -uint8_t local_slot
        -Entity local_player
        -uint32_t client_tick
    }
    class GameOverState {
        -vector~ScoreEntry~ scores
    }
    IState <|.. MenuState
    IState <|.. LobbyState
    IState <|.. PlayState
    IState <|.. GameOverState

    class WorldSync {
        -unordered_map~uint32_t, Entity~ by_network_id
        -EntityFactory& factory
        +apply(const EntitySpawn&) void
        +apply(const EntityDestroy&) void
        +apply(const WorldSnapshot&) void
    }

    class InterpolationSystem {
        -Duration interpolation_delay
    }
    class PredictionSystem {
        -deque~PendingInput~ history
        +reconcile(const WorldSnapshot&) void
    }
    class StarfieldSystem {
        -vector~Star~ stars
        -float scroll_speed
    }
    class AnimationSystem
    class RenderSystem {
        -IRenderer& renderer
        -const ColorPalette& palette
    }
    class AudioSystem {
        -IAudio& audio
        -Subscription on_shot
        -Subscription on_explosion
    }
    class Hud {
        -const IRenderer& renderer
        -bool show_debug
        +draw(const Registry&, const NetStats&, float rtt) void
    }

    ISystem <|.. InterpolationSystem
    ISystem <|.. PredictionSystem
    ISystem <|.. StarfieldSystem
    ISystem <|.. AnimationSystem
    ISystem <|.. RenderSystem
    ISystem <|.. AudioSystem

    ClientApp *-- Settings
    ClientApp *-- ClientNetwork
    ClientApp *-- StateMachine
    ClientApp *-- InputMapper
    Settings *-- KeyBindings
    StateMachine "1" *-- "*" IState
    PlayState *-- Registry
    PlayState *-- SystemScheduler
    PlayState *-- WorldSync
    PlayState *-- Hud
    PlayState --> ClientNetwork : send Input / poll
    ClientNetwork *-- NetworkThread
    ClientNetwork *-- Connection
```

---

## 9. Cross-layer overview (who depends on whom)

```mermaid
classDiagram
    direction TB

    namespace engine {
        class Registry
        class SystemScheduler
        class EventBus
        class GameLoop
        class StateMachine
        class ResourceManager
        class IRenderer
        class IAudio
        class NetworkThread
        class Packet
        class Connection
    }
    namespace rtype_common {
        class Components
        class SimulationSystems
        class EntityFactory
        class GameData
        class Codec
    }
    namespace server {
        class ServerApp
        class RoomManager
        class GameInstance
        class SnapshotBuilder
    }
    namespace client {
        class ClientApp
        class PlayState
        class WorldSync
        class RenderSystems
    }
    namespace backends_sfml {
        class SfmlRenderer
        class SfmlAudio
    }

    SimulationSystems --> Registry
    SimulationSystems --> EventBus
    EntityFactory --> Registry
    EntityFactory --> GameData
    Codec --> Packet

    GameInstance --> SystemScheduler
    GameInstance --> SimulationSystems
    GameInstance --> EntityFactory
    GameInstance --> Codec
    SnapshotBuilder --> Components
    RoomManager --> GameInstance
    ServerApp --> RoomManager
    ServerApp --> NetworkThread
    ServerApp --> GameLoop

    PlayState --> SystemScheduler
    PlayState --> RenderSystems
    PlayState --> WorldSync
    WorldSync --> EntityFactory
    WorldSync --> Codec
    RenderSystems --> IRenderer
    RenderSystems --> IAudio
    ClientApp --> StateMachine
    ClientApp --> NetworkThread
    ClientApp --> ResourceManager
    ClientApp --> SfmlRenderer
    ClientApp --> SfmlAudio
    SfmlRenderer ..|> IRenderer
    SfmlAudio ..|> IAudio
```

Rules enforced by the CMake target graph:

- `ntype-engine` links only third-party libs (Asio; SFML only inside `ntype-engine-sfml`).
- `rtype_common` links `ntype-engine` — never SFML.
- `r-type_server` links `rtype_common` + `ntype-engine` (no SFML, `NullRenderer`/`NullAudio`).
- `r-type_client` links `rtype_common` + `ntype-engine` + `ntype-engine-sfml`.
- Nothing in `engine/` or `rtype_common/` includes a header from `server/` or `client/`.

---

## 10. Key design patterns used

| Pattern            | Where                                              |
|--------------------|----------------------------------------------------|
| Entity-Component-System | `Registry`, `ComponentStorage<T>`, `ISystem`   |
| Mediator / Observer| `EventBus` between systems, `AudioSystem` reacting to gameplay events |
| State              | `StateMachine` / `IState` (client screens, instance phases) |
| Factory            | `EntityFactory` + data-driven `GameData`           |
| Strategy (data)    | `EnemyBehavior.type` selects movement in `EnemyBehaviorSystem` (Lua later) |
| Producer/Consumer  | `ThreadSafeQueue` between network and game threads |
| RAII               | `UdpSocket`, `Subscription`, `NetworkThread` (`jthread`) |
| Adapter            | `Sfml*` backends implementing engine interfaces    |
| Snapshot / delta   | `SnapshotBuilder` (baseline for Part 2 delta compression) |
