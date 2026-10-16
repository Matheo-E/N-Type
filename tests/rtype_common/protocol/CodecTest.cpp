#include <vector>

#include <gtest/gtest.h>

#include <ntype/rtype/protocol/Codec.hpp>

using namespace ntype::rtype;
using namespace ntype::rtype::protocol;
using ntype::engine::net::DecodeError;

namespace {

Message round_trip(const Message& message) {
    const auto encoded = encode(message);
    EXPECT_EQ(encoded.type, type_of(message));
    auto decoded = decode(static_cast<std::uint8_t>(encoded.type), encoded.payload);
    EXPECT_TRUE(decoded.has_value());
    return decoded.value_or(Message{});
}

} // namespace

TEST(Codec, TypeTagsFollowTheVariantOrder) {
    EXPECT_EQ(type_of(ConnectRequest{}), MessageType::ConnectRequest);
    EXPECT_EQ(type_of(Input{}), MessageType::Input);
    EXPECT_EQ(type_of(GameStateChange{}), MessageType::GameStateChange);
}

TEST(Codec, RoundTripsSimpleMessages) {
    const auto accept = std::get<ConnectAccept>(round_trip(ConnectAccept{2, 77, 1234, 60, 999}));
    EXPECT_EQ(accept.slot, 2);
    EXPECT_EQ(accept.player_network_id, 77U);
    EXPECT_EQ(accept.seed, 1234U);
    EXPECT_EQ(accept.server_tick, 999U);

    const auto input = std::get<Input>(round_trip(Input{42, input_bits::kUp | input_bits::kFire}));
    EXPECT_EQ(input.client_tick, 42U);
    EXPECT_EQ(input.bits, input_bits::kUp | input_bits::kFire);

    EXPECT_EQ(std::get<ConnectRequest>(round_trip(ConnectRequest{})).protocol_version,
              kProtocolVersion);
    EXPECT_EQ(
        std::get<ConnectRefuse>(round_trip(ConnectRefuse{RefuseReason::VersionMismatch})).reason,
        RefuseReason::VersionMismatch);
    EXPECT_EQ(std::get<Pong>(round_trip(Pong{5})).timestamp_ms, 5U);
    EXPECT_EQ(std::get<ClientLeft>(round_trip(ClientLeft{3, LeaveReason::TimedOut})).reason,
              LeaveReason::TimedOut);
    EXPECT_EQ(std::get<GameStateChange>(round_trip(GameStateChange{GameState::GameOver})).state,
              GameState::GameOver);
    EXPECT_TRUE(std::holds_alternative<Disconnect>(round_trip(Disconnect{})));
}

TEST(Codec, RoundTripsBatchesAndSnapshots) {
    EntitySpawn spawn;
    spawn.entities = {{1, EntityKind::Player, 2, -10, 20}, {2, EntityKind::EnemyMissile, 1, 5, 6}};
    const auto spawn_back = std::get<EntitySpawn>(round_trip(spawn));
    ASSERT_EQ(spawn_back.entities.size(), 2U);
    EXPECT_EQ(spawn_back.entities[1].kind, EntityKind::EnemyMissile);
    EXPECT_EQ(spawn_back.entities[0].x, -10);

    WorldSnapshot snapshot;
    snapshot.server_tick = 100;
    snapshot.last_processed_input = 98;
    snapshot.players = {{0, 3, 1500, true}, {1, 0, 20, false}};
    snapshot.entities = {{9, 100, -200, 30, -40, 2, entity_flags::kHidden}};
    const auto back = std::get<WorldSnapshot>(round_trip(snapshot));
    EXPECT_EQ(back.server_tick, 100U);
    EXPECT_EQ(back.players, snapshot.players);
    EXPECT_EQ(back.entities, snapshot.entities);
}

TEST(Codec, FullSnapshotFitsInOneDatagram) {
    WorldSnapshot snapshot;
    snapshot.players.resize(kMaxPlayers);
    snapshot.entities.resize(kMaxEntitiesPerSnapshot);
    EXPECT_LE(encode(snapshot).payload.size(), ntype::engine::net::PacketHeader::kMaxPayloadSize);

    EntitySpawn spawn;
    spawn.entities.resize(kMaxSpawnsPerMessage);
    EXPECT_LE(encode(spawn).payload.size(), ntype::engine::net::PacketHeader::kMaxPayloadSize);
}

TEST(Codec, RejectsMalformedPayloads) {
    EXPECT_EQ(decode(0, {}).error(), DecodeError::UnknownType);
    EXPECT_EQ(decode(200, {}).error(), DecodeError::UnknownType);

    auto input = encode(Input{1, 0}).payload;
    input.pop_back();
    EXPECT_EQ(decode(static_cast<std::uint8_t>(MessageType::Input), input).error(),
              DecodeError::Truncated);

    auto bad_bits = encode(Input{1, 0}).payload;
    bad_bits.back() = std::byte{0xFF};
    EXPECT_EQ(decode(static_cast<std::uint8_t>(MessageType::Input), bad_bits).error(),
              DecodeError::InvalidValue);

    auto trailing = encode(Ping{1}).payload;
    trailing.push_back(std::byte{0});
    EXPECT_EQ(decode(static_cast<std::uint8_t>(MessageType::Ping), trailing).error(),
              DecodeError::TrailingData);

    auto bad_enum = encode(GameStateChange{}).payload;
    bad_enum[0] = std::byte{42};
    EXPECT_EQ(decode(static_cast<std::uint8_t>(MessageType::GameStateChange), bad_enum).error(),
              DecodeError::InvalidValue);

    // A count larger than the data that follows must not over-read.
    const std::vector<std::byte> lying_count{std::byte{60}};
    EXPECT_EQ(decode(static_cast<std::uint8_t>(MessageType::EntitySpawn), lying_count).error(),
              DecodeError::Truncated);
}

TEST(Codec, DeliveryPolicy) {
    EXPECT_EQ(delivery_of(MessageType::EntitySpawn), ntype::engine::net::Delivery::Reliable);
    EXPECT_EQ(delivery_of(MessageType::WorldSnapshot), ntype::engine::net::Delivery::Unreliable);
    EXPECT_EQ(delivery_of(MessageType::Input), ntype::engine::net::Delivery::Unreliable);
}

TEST(Codec, QuantisationRoundsAndClamps) {
    EXPECT_EQ(quantize_position(10.25F), 21);
    EXPECT_EQ(quantize_position(-10.25F), -21);
    EXPECT_EQ(quantize_position(1e9F), 32767);
    EXPECT_FLOAT_EQ(dequantize_position(21), 10.5F);
}
