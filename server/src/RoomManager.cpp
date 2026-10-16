#include "RoomManager.hpp"

#include <algorithm>

#include <ntype/engine/util/Log.hpp>
#include <ntype/rtype/protocol/Codec.hpp>
#include <ntype/rtype/protocol/Transport.hpp>

namespace ntype::server {

namespace protocol = rtype::protocol;

RoomManager::RoomManager(const rtype::GameData& data, GameInstance::Outbox& outbound,
                         const RoomConfig config)
    : data_(data), outbound_(outbound), config_(config) {}

std::size_t RoomManager::player_count() const noexcept {
    std::size_t total = 0;
    for (const auto& instance : instances_) {
        total += instance->player_count();
    }
    return total;
}

GameInstance* RoomManager::find_or_create_room() {
    for (const auto& instance : instances_) {
        if (!instance->is_full()) {
            return instance.get();
        }
    }
    if (instances_.size() >= config_.max_rooms) {
        return nullptr;
    }
    const std::uint32_t id = next_room_id_++;
    instances_.push_back(std::make_unique<GameInstance>(id, data_, config_.base_seed + id,
                                                        outbound_, config_.instance));
    engine::log::info("room {} opened", id);
    return instances_.back().get();
}

void RoomManager::handle(const engine::net::InboundMessage& message) {
    if (const auto route = routing_.find(message.from); route != routing_.end()) {
        if (message.packet.header.type ==
            static_cast<std::uint8_t>(protocol::MessageType::ConnectRequest)) {
            static_cast<void>(route->second->join(message));
        } else {
            route->second->handle(message);
        }
        return;
    }
    // Unknown endpoints may only ask to connect; anything else is ignored.
    if (message.packet.header.type ==
        static_cast<std::uint8_t>(protocol::MessageType::ConnectRequest)) {
        accept(message);
    }
}

void RoomManager::accept(const engine::net::InboundMessage& message) {
    const auto decoded = protocol::decode(message.packet.header.type, message.packet.payload);
    if (!decoded) {
        return;
    }
    const auto& request = std::get<protocol::ConnectRequest>(*decoded);
    if (request.protocol_version != protocol::kProtocolVersion) {
        refuse(message, protocol::RefuseReason::VersionMismatch);
        return;
    }
    GameInstance* room = find_or_create_room();
    if (room == nullptr) {
        refuse(message, protocol::RefuseReason::ServerFull);
        return;
    }
    if (room->join(message)) {
        routing_[message.from] = room;
    }
}

void RoomManager::refuse(const engine::net::InboundMessage& message,
                         const protocol::RefuseReason reason) {
    // No session exists for this peer: a throwaway connection stamps the reply.
    engine::net::Connection connection(message.from, message.received_at);
    static_cast<void>(connection.on_received(message.packet.header, message.received_at));
    outbound_.push({message.from, protocol::make_packet(connection, protocol::ConnectRefuse{reason},
                                                        message.received_at)});
    engine::log::info("refused {}: {}", message.from.to_string(),
                      reason == protocol::RefuseReason::ServerFull ? "server full"
                                                                   : "protocol version mismatch");
}

void RoomManager::tick(const float dt, const engine::time::TimePoint now) {
    for (const auto& instance : instances_) {
        instance->tick_once(dt, now);
        for (const auto& endpoint : instance->take_departed()) {
            routing_.erase(endpoint);
        }
    }
    // Keep the first room alive for the next players, close the other empty ones.
    if (instances_.size() > 1) {
        const auto empty = std::stable_partition(instances_.begin() + 1, instances_.end(),
                                                 [](const std::unique_ptr<GameInstance>& instance) {
                                                     return instance->player_count() > 0;
                                                 });
        for (auto it = empty; it != instances_.end(); ++it) {
            engine::log::info("room {} closed", (*it)->id());
        }
        instances_.erase(empty, instances_.end());
    }
}

} // namespace ntype::server
