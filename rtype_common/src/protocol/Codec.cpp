#include <optional>
#include <type_traits>

#include <ntype/engine/net/ByteBuffer.hpp>
#include <ntype/rtype/protocol/Codec.hpp>

namespace ntype::rtype::protocol {

namespace {

using engine::net::ByteBuffer;
using engine::net::DecodeError;

template <typename T>
using Result = std::expected<T, DecodeError>;

// --- writers ------------------------------------------------------------------

template <typename E>
    requires std::is_enum_v<E>
void put(ByteBuffer& out, const E value) {
    out.write(static_cast<std::underlying_type_t<E>>(value));
}

template <typename T>
    requires std::is_integral_v<T>
void put(ByteBuffer& out, const T value) {
    out.write(value);
}

void write(ByteBuffer& out, const ConnectRequest& message) {
    put(out, message.protocol_version);
}
void write(ByteBuffer&, const Disconnect&) {}
void write(ByteBuffer& out, const Input& message) {
    put(out, message.client_tick);
    put(out, message.bits);
}
void write(ByteBuffer& out, const Ping& message) {
    put(out, message.timestamp_ms);
}
void write(ByteBuffer& out, const Pong& message) {
    put(out, message.timestamp_ms);
}
void write(ByteBuffer& out, const ConnectAccept& message) {
    put(out, message.slot);
    put(out, message.player_network_id);
    put(out, message.seed);
    put(out, message.tick_rate);
    put(out, message.server_tick);
}
void write(ByteBuffer& out, const ConnectRefuse& message) {
    put(out, message.reason);
}
void write(ByteBuffer& out, const EntitySpawn& message) {
    put(out, static_cast<std::uint8_t>(message.entities.size()));
    for (const auto& entity : message.entities) {
        put(out, entity.network_id);
        put(out, entity.kind);
        put(out, entity.subtype);
        put(out, entity.x);
        put(out, entity.y);
    }
}
void write(ByteBuffer& out, const EntityDestroy& message) {
    put(out, static_cast<std::uint8_t>(message.entities.size()));
    for (const auto& entity : message.entities) {
        put(out, entity.network_id);
        put(out, entity.reason);
    }
}
void write(ByteBuffer& out, const WorldSnapshot& message) {
    put(out, message.server_tick);
    put(out, message.last_processed_input);
    put(out, static_cast<std::uint8_t>(message.players.size()));
    for (const auto& player : message.players) {
        put(out, player.slot);
        put(out, player.lives);
        put(out, player.score);
        put(out, static_cast<std::uint8_t>(player.alive ? 1 : 0));
    }
    put(out, static_cast<std::uint16_t>(message.entities.size()));
    for (const auto& entity : message.entities) {
        put(out, entity.network_id);
        put(out, entity.x);
        put(out, entity.y);
        put(out, entity.vx);
        put(out, entity.vy);
        put(out, entity.hp);
        put(out, entity.flags);
    }
}
void write(ByteBuffer& out, const PlayerEvent& message) {
    put(out, message.slot);
    put(out, message.type);
    put(out, message.value);
}
void write(ByteBuffer& out, const ClientLeft& message) {
    put(out, message.slot);
    put(out, message.reason);
}
void write(ByteBuffer& out, const GameStateChange& message) {
    put(out, message.state);
}

// --- readers ------------------------------------------------------------------

/// Reads an integral field into `target`; returns the error to propagate.
template <typename T>
    requires std::is_integral_v<T>
std::optional<DecodeError> get(ByteBuffer& in, T& target) {
    const auto value = in.read<T>();
    if (!value) {
        return value.error();
    }
    target = *value;
    return std::nullopt;
}

/// Reads an enum stored on one byte and checks it is within [first, last].
template <typename E>
    requires std::is_enum_v<E>
std::optional<DecodeError> get(ByteBuffer& in, E& target, const E first, const E last) {
    std::underlying_type_t<E> raw{};
    if (const auto error = get(in, raw)) {
        return error;
    }
    if (raw < static_cast<std::underlying_type_t<E>>(first) ||
        raw > static_cast<std::underlying_type_t<E>>(last)) {
        return DecodeError::InvalidValue;
    }
    target = static_cast<E>(raw);
    return std::nullopt;
}

#define NTYPE_TRY(expression)                                                                      \
    if (const auto ntype_error = (expression)) {                                                   \
        return std::unexpected(*ntype_error);                                                      \
    }

Result<ConnectRequest> read_connect_request(ByteBuffer& in) {
    ConnectRequest message;
    NTYPE_TRY(get(in, message.protocol_version));
    return message;
}

Result<Input> read_input(ByteBuffer& in) {
    Input message;
    NTYPE_TRY(get(in, message.client_tick));
    NTYPE_TRY(get(in, message.bits));
    if ((message.bits & ~input_bits::kAll) != 0) {
        return std::unexpected(DecodeError::InvalidValue);
    }
    return message;
}

template <typename T>
Result<T> read_timestamp(ByteBuffer& in) {
    T message;
    NTYPE_TRY(get(in, message.timestamp_ms));
    return message;
}

Result<ConnectAccept> read_connect_accept(ByteBuffer& in) {
    ConnectAccept message;
    NTYPE_TRY(get(in, message.slot));
    NTYPE_TRY(get(in, message.player_network_id));
    NTYPE_TRY(get(in, message.seed));
    NTYPE_TRY(get(in, message.tick_rate));
    NTYPE_TRY(get(in, message.server_tick));
    if (message.slot >= kMaxPlayers || message.tick_rate == 0) {
        return std::unexpected(DecodeError::InvalidValue);
    }
    return message;
}

Result<ConnectRefuse> read_connect_refuse(ByteBuffer& in) {
    ConnectRefuse message;
    NTYPE_TRY(get(in, message.reason, RefuseReason::ServerFull, RefuseReason::VersionMismatch));
    return message;
}

Result<EntitySpawn> read_entity_spawn(ByteBuffer& in) {
    std::uint8_t count = 0;
    NTYPE_TRY(get(in, count));
    if (count > kMaxSpawnsPerMessage) {
        return std::unexpected(DecodeError::InvalidValue);
    }
    EntitySpawn message;
    message.entities.resize(count);
    for (auto& entity : message.entities) {
        NTYPE_TRY(get(in, entity.network_id));
        NTYPE_TRY(get(in, entity.kind, EntityKind::Player, EntityKind::EnemyMissile));
        NTYPE_TRY(get(in, entity.subtype));
        NTYPE_TRY(get(in, entity.x));
        NTYPE_TRY(get(in, entity.y));
    }
    return message;
}

Result<EntityDestroy> read_entity_destroy(ByteBuffer& in) {
    std::uint8_t count = 0;
    NTYPE_TRY(get(in, count));
    if (count > kMaxDestroysPerMessage) {
        return std::unexpected(DecodeError::InvalidValue);
    }
    EntityDestroy message;
    message.entities.resize(count);
    for (auto& entity : message.entities) {
        NTYPE_TRY(get(in, entity.network_id));
        NTYPE_TRY(get(in, entity.reason, DestroyReason::Despawned, DestroyReason::Expired));
    }
    return message;
}

Result<WorldSnapshot> read_world_snapshot(ByteBuffer& in) {
    WorldSnapshot message;
    NTYPE_TRY(get(in, message.server_tick));
    NTYPE_TRY(get(in, message.last_processed_input));
    std::uint8_t player_count = 0;
    NTYPE_TRY(get(in, player_count));
    if (player_count > kMaxPlayers) {
        return std::unexpected(DecodeError::InvalidValue);
    }
    message.players.resize(player_count);
    for (auto& player : message.players) {
        std::uint8_t alive = 0;
        NTYPE_TRY(get(in, player.slot));
        NTYPE_TRY(get(in, player.lives));
        NTYPE_TRY(get(in, player.score));
        NTYPE_TRY(get(in, alive));
        if (player.slot >= kMaxPlayers || alive > 1) {
            return std::unexpected(DecodeError::InvalidValue);
        }
        player.alive = alive == 1;
    }
    std::uint16_t entity_count = 0;
    NTYPE_TRY(get(in, entity_count));
    if (entity_count > kMaxEntitiesPerSnapshot) {
        return std::unexpected(DecodeError::InvalidValue);
    }
    message.entities.resize(entity_count);
    for (auto& entity : message.entities) {
        NTYPE_TRY(get(in, entity.network_id));
        NTYPE_TRY(get(in, entity.x));
        NTYPE_TRY(get(in, entity.y));
        NTYPE_TRY(get(in, entity.vx));
        NTYPE_TRY(get(in, entity.vy));
        NTYPE_TRY(get(in, entity.hp));
        NTYPE_TRY(get(in, entity.flags));
    }
    return message;
}

Result<PlayerEvent> read_player_event(ByteBuffer& in) {
    PlayerEvent message;
    NTYPE_TRY(get(in, message.slot));
    NTYPE_TRY(get(in, message.type, PlayerEventType::Died, PlayerEventType::Respawned));
    NTYPE_TRY(get(in, message.value));
    if (message.slot >= kMaxPlayers) {
        return std::unexpected(DecodeError::InvalidValue);
    }
    return message;
}

Result<ClientLeft> read_client_left(ByteBuffer& in) {
    ClientLeft message;
    NTYPE_TRY(get(in, message.slot));
    NTYPE_TRY(get(in, message.reason, LeaveReason::Disconnected, LeaveReason::TimedOut));
    if (message.slot >= kMaxPlayers) {
        return std::unexpected(DecodeError::InvalidValue);
    }
    return message;
}

Result<GameStateChange> read_game_state_change(ByteBuffer& in) {
    GameStateChange message;
    NTYPE_TRY(get(in, message.state, GameState::Waiting, GameState::GameOver));
    return message;
}

#undef NTYPE_TRY

Result<Message> read_message(const MessageType type, ByteBuffer& in) {
    const auto widen = [](auto result) -> Result<Message> {
        if (!result) {
            return std::unexpected(result.error());
        }
        return Message{std::move(*result)};
    };
    switch (type) {
    case MessageType::ConnectRequest:
        return widen(read_connect_request(in));
    case MessageType::ConnectAccept:
        return widen(read_connect_accept(in));
    case MessageType::ConnectRefuse:
        return widen(read_connect_refuse(in));
    case MessageType::Disconnect:
        return Message{Disconnect{}};
    case MessageType::Input:
        return widen(read_input(in));
    case MessageType::Ping:
        return widen(read_timestamp<Ping>(in));
    case MessageType::Pong:
        return widen(read_timestamp<Pong>(in));
    case MessageType::EntitySpawn:
        return widen(read_entity_spawn(in));
    case MessageType::EntityDestroy:
        return widen(read_entity_destroy(in));
    case MessageType::WorldSnapshot:
        return widen(read_world_snapshot(in));
    case MessageType::PlayerEvent:
        return widen(read_player_event(in));
    case MessageType::ClientLeft:
        return widen(read_client_left(in));
    case MessageType::GameStateChange:
        return widen(read_game_state_change(in));
    }
    return std::unexpected(DecodeError::UnknownType);
}

} // namespace

MessageType type_of(const Message& message) noexcept {
    // The variant alternatives are declared in MessageType order.
    return static_cast<MessageType>(message.index() + 1);
}

EncodedMessage encode(const Message& message) {
    ByteBuffer out;
    std::visit([&out](const auto& concrete) { write(out, concrete); }, message);
    return {type_of(message), std::move(out).release()};
}

std::expected<Message, DecodeError> decode(const std::uint8_t type,
                                           const std::span<const std::byte> payload) {
    if (type < static_cast<std::uint8_t>(MessageType::ConnectRequest) ||
        type > static_cast<std::uint8_t>(MessageType::GameStateChange)) {
        return std::unexpected(DecodeError::UnknownType);
    }
    ByteBuffer in(payload);
    auto message = read_message(static_cast<MessageType>(type), in);
    if (message && in.remaining() != 0) {
        return std::unexpected(DecodeError::TrailingData);
    }
    return message;
}

engine::net::Delivery delivery_of(const MessageType type) noexcept {
    switch (type) {
    case MessageType::ConnectAccept:
    case MessageType::EntitySpawn:
    case MessageType::EntityDestroy:
    case MessageType::PlayerEvent:
    case MessageType::ClientLeft:
    case MessageType::GameStateChange:
        return engine::net::Delivery::Reliable;
    default:
        return engine::net::Delivery::Unreliable;
    }
}

} // namespace ntype::rtype::protocol
