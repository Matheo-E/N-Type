#pragma once

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

#include <ntype/engine/net/Endpoint.hpp>

#include "GameInstance.hpp"

namespace ntype::server {

struct RoomConfig {
    unsigned max_rooms = 4;
    std::uint32_t base_seed = 0;
    InstanceConfig instance;
};

/// Routes packets to the game instance owning their endpoint and creates
/// rooms as players arrive. A full room makes the next player open a new one.
class RoomManager {
public:
    RoomManager(const rtype::GameData& data, GameInstance::Outbox& outbound, RoomConfig config);

    void handle(const engine::net::InboundMessage& message);
    void tick(float dt, engine::time::TimePoint now);

    [[nodiscard]] std::size_t room_count() const noexcept { return instances_.size(); }
    [[nodiscard]] std::size_t player_count() const noexcept;

private:
    GameInstance* find_or_create_room();
    void accept(const engine::net::InboundMessage& message);
    void refuse(const engine::net::InboundMessage& message, rtype::protocol::RefuseReason reason);

    const rtype::GameData& data_;
    GameInstance::Outbox& outbound_;
    RoomConfig config_;
    std::uint32_t next_room_id_ = 1;
    std::vector<std::unique_ptr<GameInstance>> instances_;
    std::unordered_map<engine::net::Endpoint, GameInstance*> routing_;
};

} // namespace ntype::server
