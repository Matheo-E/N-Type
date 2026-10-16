#include "GameInstance.hpp"

#include <algorithm>
#include <chrono>

#include <ntype/engine/util/Log.hpp>
#include <ntype/rtype/Components.hpp>
#include <ntype/rtype/Events.hpp>
#include <ntype/rtype/protocol/Transport.hpp>
#include <ntype/rtype/systems/Systems.hpp>

namespace ntype::server {

namespace protocol = rtype::protocol;
using engine::net::Endpoint;
using engine::net::InboundMessage;
using engine::time::TimePoint;

GameInstance::GameInstance(const std::uint32_t id, const rtype::GameData& data,
                           const std::uint32_t seed, Outbox& outbound, const InstanceConfig config)
    : id_(id), data_(data), seed_(seed), outbound_(outbound), config_(config), rng_(seed),
      factory_(registry_, data) {
    subscriptions_.push_back(
        events_.subscribe<rtype::EntityDestroyed>([this](const rtype::EntityDestroyed& event) {
            if (event.network_id != 0 && announced_.erase(event.network_id) > 0) {
                pending_destroys_.push_back({event.network_id, event.reason});
            }
        }));
    subscriptions_.push_back(
        events_.subscribe<rtype::PlayerDied>([this](const rtype::PlayerDied& event) {
            pending_events_.emplace_back(protocol::PlayerEvent{
                event.slot, protocol::PlayerEventType::Died, event.lives_left});
        }));
    subscriptions_.push_back(
        events_.subscribe<rtype::PlayerRespawned>([this](const rtype::PlayerRespawned& event) {
            pending_events_.emplace_back(
                protocol::PlayerEvent{event.slot, protocol::PlayerEventType::Respawned, 0});
        }));
    build_world();
}

void GameInstance::build_world() {
    namespace systems = rtype::systems;
    scheduler_.clear();
    scheduler_.add<systems::PlayerControlSystem>(data_);
    scheduler_.add<systems::EnemyBehaviorSystem>();
    scheduler_.add<systems::WeaponSystem>(factory_);
    scheduler_.add<systems::MovementSystem>();
    scheduler_.add<systems::BoundsSystem>(data_.world_size);
    scheduler_.add<systems::CollisionSystem>(events_);
    scheduler_.add<systems::DamageSystem>(registry_, events_, data_);
    scheduler_.add<systems::LifetimeSystem>();
    scheduler_.add<systems::PlayerLifeSystem>(factory_, events_);
    scheduler_.add<systems::SpawnSystem>(factory_, data_, rng_);
    scheduler_.add<systems::CleanupSystem>(events_);
}

void GameInstance::reset_world() {
    for (const std::uint32_t network_id : announced_) {
        pending_destroys_.push_back({network_id, rtype::DestroyReason::Despawned});
    }
    // Sorted so the destroy order does not depend on the hash set layout.
    std::ranges::sort(pending_destroys_, {}, &protocol::DestroyedEntity::network_id);
    announced_.clear();
    registry_.clear();
    build_world();
    for (auto& session : sessions_) {
        if (session) {
            session->player = factory_.create_player(session->slot);
        }
    }
}

std::size_t GameInstance::player_count() const noexcept {
    return static_cast<std::size_t>(
        std::ranges::count_if(sessions_, [](const auto& session) { return session.has_value(); }));
}

bool GameInstance::has_session(const Endpoint& endpoint) const noexcept {
    return std::ranges::any_of(sessions_, [&](const auto& session) {
        return session && session->connection.remote() == endpoint;
    });
}

ClientSession* GameInstance::find_session(const Endpoint& endpoint) noexcept {
    for (auto& session : sessions_) {
        if (session && session->connection.remote() == endpoint) {
            return &*session;
        }
    }
    return nullptr;
}

std::expected<std::uint8_t, protocol::RefuseReason>
GameInstance::join(const InboundMessage& request) {
    if (ClientSession* existing = find_session(request.from)) {
        // Retransmitted request: our acceptance was probably lost.
        handle(request);
        return existing->slot;
    }
    const auto free_slot =
        std::ranges::find_if(sessions_, [](const auto& session) { return !session.has_value(); });
    if (free_slot == sessions_.end()) {
        return std::unexpected(protocol::RefuseReason::ServerFull);
    }
    const auto slot = static_cast<std::uint8_t>(free_slot - sessions_.begin());
    auto& session = free_slot->emplace(ClientSession{
        engine::net::Connection(request.from, request.received_at), slot, {}, 0, false});
    session.player = factory_.create_player(slot);
    // Feed the request through the channel so our first ack covers it.
    static_cast<void>(protocol::receive(session.connection, request.packet, request.received_at));

    const auto network_id = registry_.get<rtype::NetworkId>(session.player).id;
    send(session,
         protocol::ConnectAccept{slot, network_id, seed_,
                                 static_cast<std::uint16_t>(config_.tick_rate), tick_},
         request.received_at);
    send_full_world(session, request.received_at);
    engine::log::info("room {}: player {} joined from {}", id_, slot + 1, request.from.to_string());
    return slot;
}

void GameInstance::send_full_world(ClientSession& session, const TimePoint now) {
    send(session, protocol::GameStateChange{public_state()}, now);
    protocol::EntitySpawn batch;
    for (const auto entity :
         registry_.view<rtype::NetworkId, rtype::EntityType, rtype::Transform>().entities()) {
        if (!announced_.contains(registry_.get<rtype::NetworkId>(entity).id)) {
            continue;
        }
        batch.entities.push_back(SnapshotBuilder::spawn_message(registry_, entity));
        if (batch.entities.size() == protocol::kMaxSpawnsPerMessage) {
            send(session, batch, now);
            batch.entities.clear();
        }
    }
    if (!batch.entities.empty()) {
        send(session, batch, now);
    }
}

void GameInstance::handle(const InboundMessage& message) {
    ClientSession* session = find_session(message.from);
    if (session == nullptr) {
        return;
    }
    const auto decoded =
        protocol::receive(session->connection, message.packet, message.received_at);
    if (decoded) {
        dispatch(*session, *decoded, message.received_at);
    }
}

void GameInstance::dispatch(ClientSession& session, const protocol::Message& message,
                            const TimePoint now) {
    if (const auto* input = std::get_if<protocol::Input>(&message)) {
        // Inputs are unreliable and may arrive out of order: keep the newest.
        if (session.has_input && input->client_tick <= session.last_input_tick) {
            return;
        }
        session.has_input = true;
        session.last_input_tick = input->client_tick;
        if (auto* player_input = registry_.try_get<rtype::PlayerInput>(session.player)) {
            player_input->bits = input->bits;
            player_input->client_tick = input->client_tick;
        }
    } else if (const auto* ping = std::get_if<protocol::Ping>(&message)) {
        send(session, protocol::Pong{ping->timestamp_ms}, now);
    } else if (std::holds_alternative<protocol::Disconnect>(message)) {
        leave(session, protocol::LeaveReason::Disconnected, now);
    }
    // Anything else from a client (server-only messages) is ignored.
}

void GameInstance::leave(ClientSession& session, const protocol::LeaveReason reason,
                         const TimePoint now) {
    const std::uint8_t slot = session.slot;
    engine::log::info("room {}: player {} left ({})", id_, slot + 1,
                      reason == protocol::LeaveReason::TimedOut ? "timed out" : "disconnected");
    if (registry_.is_alive(session.player)) {
        const auto network_id = registry_.get<rtype::NetworkId>(session.player).id;
        if (announced_.erase(network_id) > 0) {
            pending_destroys_.push_back({network_id, rtype::DestroyReason::Despawned});
        }
        registry_.destroy(session.player);
    }
    departed_.push_back(session.connection.remote());
    sessions_[slot].reset();
    broadcast(protocol::ClientLeft{slot, reason}, now);
}

void GameInstance::check_timeouts(const TimePoint now) {
    for (auto& session : sessions_) {
        if (session && session->connection.timed_out(now, config_.client_timeout)) {
            leave(*session, protocol::LeaveReason::TimedOut, now);
        }
    }
}

protocol::GameState GameInstance::public_state() const noexcept {
    switch (phase_) {
    case InstancePhase::Running:
        return protocol::GameState::Running;
    case InstancePhase::GameOver:
        return protocol::GameState::GameOver;
    case InstancePhase::Waiting:
        break;
    }
    return protocol::GameState::Waiting;
}

void GameInstance::check_game_over(const TimePoint now) {
    bool anyone = false;
    bool all_out = true;
    registry_.view<rtype::Player>().each([&](rtype::Entity, const rtype::Player& player) {
        anyone = true;
        all_out = all_out && !player.alive && player.lives == 0;
    });
    if (anyone && all_out) {
        phase_ = InstancePhase::GameOver;
        game_over_timer_ = config_.game_over_delay.count();
        broadcast(protocol::GameStateChange{protocol::GameState::GameOver}, now);
        engine::log::info("room {}: game over at tick {}", id_, tick_);
    }
}

void GameInstance::tick_once(const float dt, const TimePoint now) {
    check_timeouts(now);
    if (player_count() == 0) {
        if (phase_ != InstancePhase::Waiting) {
            reset_world();
            pending_destroys_.clear();
            phase_ = InstancePhase::Waiting;
        }
        return;
    }
    if (phase_ == InstancePhase::Waiting) {
        phase_ = InstancePhase::Running;
        broadcast(protocol::GameStateChange{protocol::GameState::Running}, now);
    }

    scheduler_.update(registry_, dt);
    events_.dispatch_queued();
    ++tick_;

    if (phase_ == InstancePhase::Running) {
        check_game_over(now);
    } else if (phase_ == InstancePhase::GameOver) {
        game_over_timer_ -= static_cast<double>(dt);
        if (game_over_timer_ <= 0.0) {
            reset_world();
            phase_ = InstancePhase::Running;
            broadcast(protocol::GameStateChange{protocol::GameState::Running}, now);
        }
    }

    announce_entities(now);
    for (const auto& event : std::exchange(pending_events_, {})) {
        broadcast(event, now);
    }
    if (tick_ % config_.snapshot_interval == 0) {
        send_snapshots(now);
    }
    flush_resends(now);
}

void GameInstance::announce_entities(const TimePoint now) {
    // Destroys first, so a client never sees an id it already removed.
    for (std::size_t offset = 0; offset < pending_destroys_.size();
         offset += protocol::kMaxDestroysPerMessage) {
        const auto end =
            std::min(pending_destroys_.size(), offset + protocol::kMaxDestroysPerMessage);
        broadcast(
            protocol::EntityDestroy{
                {pending_destroys_.begin() + static_cast<std::ptrdiff_t>(offset),
                 pending_destroys_.begin() + static_cast<std::ptrdiff_t>(end)}},
            now);
    }
    pending_destroys_.clear();

    protocol::EntitySpawn batch;
    for (const auto entity :
         registry_.view<rtype::NetworkId, rtype::EntityType, rtype::Transform>().entities()) {
        const auto network_id = registry_.get<rtype::NetworkId>(entity).id;
        if (!announced_.insert(network_id).second) {
            continue;
        }
        batch.entities.push_back(SnapshotBuilder::spawn_message(registry_, entity));
        if (batch.entities.size() == protocol::kMaxSpawnsPerMessage) {
            broadcast(batch, now);
            batch.entities.clear();
        }
    }
    if (!batch.entities.empty()) {
        broadcast(batch, now);
    }
}

void GameInstance::send_snapshots(const TimePoint now) {
    auto snapshots = snapshots_.build(registry_, tick_, announced_);
    for (auto& session : sessions_) {
        if (!session) {
            continue;
        }
        for (auto& snapshot : snapshots) {
            snapshot.last_processed_input = session->last_input_tick;
            send(*session, snapshot, now);
        }
    }
}

void GameInstance::flush_resends(const TimePoint now) {
    for (auto& session : sessions_) {
        if (!session) {
            continue;
        }
        for (auto& packet : session->connection.collect_resends(now)) {
            outbound_.push({session->connection.remote(), std::move(packet)});
        }
    }
}

void GameInstance::send(ClientSession& session, const protocol::Message& message,
                        const TimePoint now) {
    outbound_.push(
        {session.connection.remote(), protocol::make_packet(session.connection, message, now)});
}

void GameInstance::broadcast(const protocol::Message& message, const TimePoint now) {
    for (auto& session : sessions_) {
        if (session) {
            send(*session, message, now);
        }
    }
}

std::vector<Endpoint> GameInstance::take_departed() {
    return std::exchange(departed_, {});
}

} // namespace ntype::server
