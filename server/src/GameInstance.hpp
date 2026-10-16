#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <unordered_set>
#include <vector>

#include <ntype/engine/ecs/Registry.hpp>
#include <ntype/engine/ecs/SystemScheduler.hpp>
#include <ntype/engine/events/EventBus.hpp>
#include <ntype/engine/math/Random.hpp>
#include <ntype/engine/net/NetMessages.hpp>
#include <ntype/engine/net/ThreadSafeQueue.hpp>
#include <ntype/rtype/EntityFactory.hpp>
#include <ntype/rtype/GameData.hpp>
#include <ntype/rtype/protocol/Messages.hpp>

#include "ClientSession.hpp"
#include "SnapshotBuilder.hpp"

namespace ntype::server {

enum class InstancePhase { Waiting, Running, GameOver };

struct InstanceConfig {
    unsigned tick_rate = 60;
    unsigned snapshot_interval = 2;
    engine::time::Duration client_timeout{5.0};
    engine::time::Duration game_over_delay{5.0};
};

/// One authoritative game: up to four players, its own world and rules.
///
/// Per tick: inputs already applied by handle(), then the systems run, then
/// spawned/destroyed entities are announced reliably and a snapshot is sent.
class GameInstance {
public:
    static constexpr std::size_t kMaxPlayers = rtype::protocol::kMaxPlayers;
    using Outbox = engine::net::ThreadSafeQueue<engine::net::OutboundMessage>;

    GameInstance(std::uint32_t id, const rtype::GameData& data, std::uint32_t seed,
                 Outbox& outbound, InstanceConfig config);

    GameInstance(const GameInstance&) = delete;
    GameInstance& operator=(const GameInstance&) = delete;
    GameInstance(GameInstance&&) = delete;
    GameInstance& operator=(GameInstance&&) = delete;

    /// Adds a player for `request` (a ConnectRequest packet) and answers it.
    /// Joining twice from the same endpoint just re-sends the acceptance.
    std::expected<std::uint8_t, rtype::protocol::RefuseReason>
    join(const engine::net::InboundMessage& request);
    /// Handles a packet from an endpoint that already has a session.
    void handle(const engine::net::InboundMessage& message);
    void tick_once(float dt, engine::time::TimePoint now);

    /// Endpoints whose session ended since the last call.
    [[nodiscard]] std::vector<engine::net::Endpoint> take_departed();

    [[nodiscard]] std::uint32_t id() const noexcept { return id_; }
    [[nodiscard]] InstancePhase phase() const noexcept { return phase_; }
    [[nodiscard]] std::uint32_t tick() const noexcept { return tick_; }
    [[nodiscard]] std::size_t player_count() const noexcept;
    [[nodiscard]] bool is_full() const noexcept { return player_count() == kMaxPlayers; }
    [[nodiscard]] bool has_session(const engine::net::Endpoint& endpoint) const noexcept;
    /// Read access for tests and tools.
    [[nodiscard]] engine::ecs::Registry& registry() noexcept { return registry_; }

private:
    ClientSession* find_session(const engine::net::Endpoint& endpoint) noexcept;
    void dispatch(ClientSession& session, const rtype::protocol::Message& message,
                  engine::time::TimePoint now);
    void leave(ClientSession& session, rtype::protocol::LeaveReason reason,
               engine::time::TimePoint now);
    void check_timeouts(engine::time::TimePoint now);
    void build_world();
    void reset_world();
    void check_game_over(engine::time::TimePoint now);
    void announce_entities(engine::time::TimePoint now);
    void send_snapshots(engine::time::TimePoint now);
    void flush_resends(engine::time::TimePoint now);
    void send(ClientSession& session, const rtype::protocol::Message& message,
              engine::time::TimePoint now);
    void broadcast(const rtype::protocol::Message& message, engine::time::TimePoint now);
    void send_full_world(ClientSession& session, engine::time::TimePoint now);
    [[nodiscard]] rtype::protocol::GameState public_state() const noexcept;

    std::uint32_t id_;
    const rtype::GameData& data_;
    std::uint32_t seed_;
    Outbox& outbound_;
    InstanceConfig config_;

    engine::ecs::Registry registry_;
    engine::events::EventBus events_;
    engine::math::Random rng_;
    rtype::EntityFactory factory_;
    engine::ecs::SystemScheduler scheduler_;
    std::vector<engine::events::Subscription> subscriptions_;
    SnapshotBuilder snapshots_;

    std::array<std::optional<ClientSession>, kMaxPlayers> sessions_;
    std::vector<engine::net::Endpoint> departed_;
    std::unordered_set<std::uint32_t> announced_;
    std::vector<rtype::protocol::DestroyedEntity> pending_destroys_;
    std::vector<rtype::protocol::Message> pending_events_;

    InstancePhase phase_ = InstancePhase::Waiting;
    std::uint32_t tick_ = 0;
    double game_over_timer_ = 0.0;
};

} // namespace ntype::server
