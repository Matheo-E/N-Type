#pragma once

// R-Type protocol messages. The byte layout of each one is specified in
// docs/PROTOCOL.md; the codec in Codec.cpp is the reference implementation.

#include <cstdint>
#include <variant>
#include <vector>

#include <ntype/rtype/Components.hpp>

namespace ntype::rtype::protocol {

inline constexpr std::uint16_t kProtocolVersion = 1;

enum class MessageType : std::uint8_t {
    ConnectRequest = 1,
    ConnectAccept = 2,
    ConnectRefuse = 3,
    Disconnect = 4,
    Input = 5,
    Ping = 6,
    Pong = 7,
    EntitySpawn = 8,
    EntityDestroy = 9,
    WorldSnapshot = 10,
    PlayerEvent = 11,
    ClientLeft = 12,
    GameStateChange = 13,
};

enum class RefuseReason : std::uint8_t { ServerFull = 1, VersionMismatch = 2 };
enum class PlayerEventType : std::uint8_t { Died = 1, Respawned = 2 };
enum class LeaveReason : std::uint8_t { Disconnected = 1, TimedOut = 2 };
enum class GameState : std::uint8_t { Waiting = 1, Running = 2, GameOver = 3 };

namespace entity_flags {
inline constexpr std::uint8_t kHidden = 1U << 0U;
inline constexpr std::uint8_t kInvulnerable = 1U << 1U;
} // namespace entity_flags

// --- Client to server -------------------------------------------------------

struct ConnectRequest {
    std::uint16_t protocol_version = kProtocolVersion;
};

struct Disconnect {};

struct Input {
    std::uint32_t client_tick = 0;
    std::uint8_t bits = 0;
};

struct Ping {
    std::uint32_t timestamp_ms = 0;
};

// --- Server to client -------------------------------------------------------

struct ConnectAccept {
    std::uint8_t slot = 0;
    std::uint32_t player_network_id = 0;
    std::uint32_t seed = 0;
    std::uint16_t tick_rate = 60;
    std::uint32_t server_tick = 0;
};

struct ConnectRefuse {
    RefuseReason reason = RefuseReason::ServerFull;
};

struct Pong {
    std::uint32_t timestamp_ms = 0;
};

struct SpawnedEntity {
    std::uint32_t network_id = 0;
    EntityKind kind = EntityKind::Enemy;
    std::uint8_t subtype = 0;
    std::int16_t x = 0;
    std::int16_t y = 0;
};

/// Batch of new entities (reliable).
struct EntitySpawn {
    std::vector<SpawnedEntity> entities;
};

struct DestroyedEntity {
    std::uint32_t network_id = 0;
    DestroyReason reason = DestroyReason::Despawned;
};

/// Batch of removed entities (reliable).
struct EntityDestroy {
    std::vector<DestroyedEntity> entities;
};

/// Quantised state of one entity: positions in half pixels, velocities in
/// pixels per second.
struct EntityState {
    std::uint32_t network_id = 0;
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::int16_t vx = 0;
    std::int16_t vy = 0;
    std::uint8_t hp = 0;
    std::uint8_t flags = 0;

    friend bool operator==(const EntityState&, const EntityState&) = default;
};

struct PlayerState {
    std::uint8_t slot = 0;
    std::uint8_t lives = 0;
    std::uint32_t score = 0;
    bool alive = false;

    friend bool operator==(const PlayerState&, const PlayerState&) = default;
};

/// Unreliable world state. Large worlds are split over several snapshots
/// carrying the same server tick.
struct WorldSnapshot {
    std::uint32_t server_tick = 0;
    std::uint32_t last_processed_input = 0;
    std::vector<PlayerState> players;
    std::vector<EntityState> entities;
};

struct PlayerEvent {
    std::uint8_t slot = 0;
    PlayerEventType type = PlayerEventType::Died;
    std::uint32_t value = 0;
};

struct ClientLeft {
    std::uint8_t slot = 0;
    LeaveReason reason = LeaveReason::Disconnected;
};

struct GameStateChange {
    GameState state = GameState::Waiting;
};

using Message = std::variant<ConnectRequest, ConnectAccept, ConnectRefuse, Disconnect, Input, Ping,
                             Pong, EntitySpawn, EntityDestroy, WorldSnapshot, PlayerEvent,
                             ClientLeft, GameStateChange>;

/// Wire limits derived from the maximum payload size.
inline constexpr std::size_t kMaxSpawnsPerMessage = 64;
inline constexpr std::size_t kMaxDestroysPerMessage = 128;
inline constexpr std::size_t kMaxEntitiesPerSnapshot = 80;
inline constexpr std::size_t kMaxPlayers = 4;

/// Position quantisation shared by the codec users.
[[nodiscard]] constexpr std::int16_t quantize_position(const float value) noexcept {
    const float scaled = value * 2.0F;
    if (scaled > 32767.0F) {
        return 32767;
    }
    if (scaled < -32768.0F) {
        return -32768;
    }
    return static_cast<std::int16_t>(scaled < 0.0F ? scaled - 0.5F : scaled + 0.5F);
}

[[nodiscard]] constexpr float dequantize_position(const std::int16_t value) noexcept {
    return static_cast<float>(value) / 2.0F;
}

[[nodiscard]] constexpr std::int16_t quantize_velocity(const float value) noexcept {
    if (value > 32767.0F) {
        return 32767;
    }
    if (value < -32768.0F) {
        return -32768;
    }
    return static_cast<std::int16_t>(value < 0.0F ? value - 0.5F : value + 0.5F);
}

} // namespace ntype::rtype::protocol
