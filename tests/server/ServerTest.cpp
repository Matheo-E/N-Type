#include <GameInstance.hpp>
#include <RoomManager.hpp>
#include <ServerConfig.hpp>
#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include <ntype/rtype/protocol/Codec.hpp>
#include <ntype/rtype/protocol/Transport.hpp>

#include "../rtype_common/TestData.hpp"

using namespace std::chrono_literals;
using namespace ntype;
using engine::net::Endpoint;
using engine::net::InboundMessage;
namespace protocol = rtype::protocol;
using ntype::server::GameInstance;

namespace {

/// A fake client: its own connection state, so packets carry proper headers.
struct FakeClient {
    Endpoint endpoint;
    engine::net::Connection connection;

    explicit FakeClient(const std::uint16_t port)
        : endpoint{0x7F000001, port}, connection(Endpoint{0x7F000001, 4242}, {}) {}

    InboundMessage packet(const protocol::Message& message, const engine::time::TimePoint now) {
        return {endpoint, protocol::make_packet(connection, message, now), now};
    }
};

std::vector<protocol::Message> messages_for(GameInstance::Outbox& outbox, const Endpoint& to) {
    std::vector<protocol::Message> result;
    for (const auto& outbound : outbox.drain()) {
        if (outbound.to == to) {
            if (auto message =
                    protocol::decode(outbound.packet.header.type, outbound.packet.payload)) {
                result.push_back(std::move(*message));
            }
        }
    }
    return result;
}

} // namespace

TEST(GameInstance, SameSeedAndInputsGiveTheSameWorld) {
    GameInstance::Outbox outbox_a;
    GameInstance::Outbox outbox_b;
    server::GameInstance a(1, test_game_data(), 99, outbox_a, {});
    server::GameInstance b(1, test_game_data(), 99, outbox_b, {});
    FakeClient client_a(5000);
    FakeClient client_b(5000);
    const engine::time::TimePoint start{};
    ASSERT_TRUE(a.join(client_a.packet(protocol::ConnectRequest{}, start)));
    ASSERT_TRUE(b.join(client_b.packet(protocol::ConnectRequest{}, start)));

    server::SnapshotBuilder builder;
    for (std::uint32_t tick = 1; tick <= 600; ++tick) {
        const auto now = start + std::chrono::milliseconds(tick * 16);
        const std::uint8_t bits = (tick / 40) % 2 == 0
                                      ? rtype::input_bits::kUp | rtype::input_bits::kFire
                                      : rtype::input_bits::kDown;
        a.handle(client_a.packet(protocol::Input{tick, bits}, now));
        b.handle(client_b.packet(protocol::Input{tick, bits}, now));
        a.tick_once(1.0F / 60.0F, now);
        b.tick_once(1.0F / 60.0F, now);
    }
    const std::unordered_set<std::uint32_t> everything = [&] {
        std::unordered_set<std::uint32_t> ids;
        a.registry().view<rtype::NetworkId>().each(
            [&](rtype::Entity, const rtype::NetworkId& id) { ids.insert(id.id); });
        return ids;
    }();
    const auto world_a = builder.build(a.registry(), a.tick(), everything);
    const auto world_b = builder.build(b.registry(), b.tick(), everything);
    ASSERT_EQ(world_a.size(), world_b.size());
    for (std::size_t i = 0; i < world_a.size(); ++i) {
        EXPECT_EQ(world_a[i].entities, world_b[i].entities);
        EXPECT_EQ(world_a[i].players, world_b[i].players);
    }
    EXPECT_GT(a.registry().alive_count(), 1U);
}

TEST(GameInstance, FifthPlayerIsRefusedAndOthersKeepPlaying) {
    GameInstance::Outbox outbox;
    server::GameInstance game(1, test_game_data(), 1, outbox, {});
    const engine::time::TimePoint now{};
    std::vector<FakeClient> clients;
    for (std::uint16_t i = 0; i < 5; ++i) {
        clients.emplace_back(static_cast<std::uint16_t>(6000 + i));
    }
    for (std::size_t i = 0; i < 4; ++i) {
        const auto slot = game.join(clients[i].packet(protocol::ConnectRequest{}, now));
        ASSERT_TRUE(slot.has_value());
        EXPECT_EQ(*slot, i);
    }
    const auto refused = game.join(clients[4].packet(protocol::ConnectRequest{}, now));
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error(), protocol::RefuseReason::ServerFull);
    game.tick_once(1.0F / 60.0F, now);
    EXPECT_EQ(game.player_count(), 4U);
}

TEST(GameInstance, JoinAnswersWithAcceptAndRetransmitIsIdempotent) {
    GameInstance::Outbox outbox;
    server::GameInstance game(1, test_game_data(), 1, outbox, {});
    FakeClient client(7000);
    const engine::time::TimePoint now{};
    ASSERT_TRUE(game.join(client.packet(protocol::ConnectRequest{}, now)));
    ASSERT_TRUE(game.join(client.packet(protocol::ConnectRequest{}, now)));
    EXPECT_EQ(game.player_count(), 1U);
    const auto messages = messages_for(outbox, client.endpoint);
    ASSERT_FALSE(messages.empty());
    EXPECT_TRUE(std::holds_alternative<protocol::ConnectAccept>(messages.front()));
}

TEST(GameInstance, SilentClientTimesOutAndOthersAreNotified) {
    GameInstance::Outbox outbox;
    server::InstanceConfig config;
    config.client_timeout = engine::time::Duration(1.0);
    server::GameInstance game(1, test_game_data(), 1, outbox, config);
    FakeClient stays(8000);
    FakeClient leaves(8001);
    const engine::time::TimePoint start{};
    ASSERT_TRUE(game.join(stays.packet(protocol::ConnectRequest{}, start)));
    ASSERT_TRUE(game.join(leaves.packet(protocol::ConnectRequest{}, start)));

    for (int tick = 1; tick <= 120; ++tick) {
        const auto now = start + std::chrono::milliseconds(tick * 16);
        game.handle(stays.packet(protocol::Input{static_cast<std::uint32_t>(tick), 0}, now));
        game.tick_once(1.0F / 60.0F, now);
    }
    EXPECT_EQ(game.player_count(), 1U);
    EXPECT_EQ(game.take_departed(), std::vector<Endpoint>{leaves.endpoint});
    bool notified = false;
    for (const auto& message : messages_for(outbox, stays.endpoint)) {
        if (const auto* left = std::get_if<protocol::ClientLeft>(&message)) {
            notified = left->slot == 1 && left->reason == protocol::LeaveReason::TimedOut;
        }
    }
    EXPECT_TRUE(notified);
}

TEST(GameInstance, SnapshotsCarryTheWorld) {
    GameInstance::Outbox outbox;
    server::GameInstance game(1, test_game_data(), 1, outbox, {});
    FakeClient client(9000);
    const engine::time::TimePoint now{};
    ASSERT_TRUE(game.join(client.packet(protocol::ConnectRequest{}, now)));
    game.tick_once(1.0F / 60.0F, now);
    game.tick_once(1.0F / 60.0F, now);
    bool spawned = false;
    bool snapshot = false;
    for (const auto& message : messages_for(outbox, client.endpoint)) {
        spawned = spawned || std::holds_alternative<protocol::EntitySpawn>(message);
        if (const auto* world = std::get_if<protocol::WorldSnapshot>(&message)) {
            snapshot = !world->entities.empty() && world->players.size() == 1;
        }
    }
    EXPECT_TRUE(spawned);
    EXPECT_TRUE(snapshot);
}

TEST(RoomManager, OpensASecondRoomWhenTheFirstIsFull) {
    GameInstance::Outbox outbox;
    server::RoomManager rooms(test_game_data(), outbox, {});
    const engine::time::TimePoint now{};
    for (std::uint16_t i = 0; i < 5; ++i) {
        FakeClient client(static_cast<std::uint16_t>(10000 + i));
        rooms.handle(client.packet(protocol::ConnectRequest{}, now));
    }
    EXPECT_EQ(rooms.room_count(), 2U);
    EXPECT_EQ(rooms.player_count(), 5U);
}

TEST(RoomManager, IgnoresStrangersAndRefusesOtherVersions) {
    GameInstance::Outbox outbox;
    server::RoomManager rooms(test_game_data(), outbox, {});
    FakeClient stranger(11000);
    const engine::time::TimePoint now{};
    rooms.handle(stranger.packet(protocol::Input{1, 0}, now));
    EXPECT_EQ(rooms.player_count(), 0U);
    EXPECT_EQ(outbox.size(), 0U);

    rooms.handle(stranger.packet(protocol::ConnectRequest{999}, now));
    const auto replies = messages_for(outbox, stranger.endpoint);
    ASSERT_EQ(replies.size(), 1U);
    EXPECT_EQ(std::get<protocol::ConnectRefuse>(replies[0]).reason,
              protocol::RefuseReason::VersionMismatch);
}

TEST(ServerConfig, ParsesOptionsAndRejectsGarbage) {
    const std::vector<std::string_view> good{"-p", "5000", "--seed", "7", "-t", "120"};
    const auto config = server::ServerConfig::from_args(good);
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->port, 5000);
    EXPECT_EQ(config->seed, 7U);
    EXPECT_EQ(config->snapshot_interval, 4U);

    const std::vector<std::string_view> bad_port{"-p", "99999"};
    EXPECT_FALSE(server::ServerConfig::from_args(bad_port).has_value());
    const std::vector<std::string_view> unknown{"--nope", "1"};
    EXPECT_FALSE(server::ServerConfig::from_args(unknown).has_value());
}
