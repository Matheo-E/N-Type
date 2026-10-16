#pragma once

#include <cstdint>
#include <unordered_map>

#include <ntype/engine/ecs/Registry.hpp>
#include <ntype/engine/events/EventBus.hpp>
#include <ntype/rtype/EntityFactory.hpp>
#include <ntype/rtype/protocol/Messages.hpp>

namespace ntype::client {

/// Mirrors the server's world: creates, updates and removes local entities
/// from EntitySpawn, WorldSnapshot and EntityDestroy messages.
class WorldSync {
public:
    WorldSync(engine::ecs::Registry& registry, rtype::EntityFactory& factory,
              engine::events::EventBus& events, float snapshot_period) noexcept
        : registry_(registry), factory_(factory), events_(events),
          snapshot_period_(snapshot_period) {}

    void apply(const rtype::protocol::EntitySpawn& message);
    void apply(const rtype::protocol::EntityDestroy& message);
    void apply(const rtype::protocol::WorldSnapshot& message);

    [[nodiscard]] engine::ecs::Entity find(std::uint32_t network_id) const noexcept;
    [[nodiscard]] std::size_t replica_count() const noexcept { return by_network_id_.size(); }
    [[nodiscard]] std::uint32_t last_server_tick() const noexcept { return last_tick_; }

private:
    engine::ecs::Registry& registry_;
    rtype::EntityFactory& factory_;
    engine::events::EventBus& events_;
    float snapshot_period_;
    std::unordered_map<std::uint32_t, engine::ecs::Entity> by_network_id_;
    std::uint32_t last_tick_ = 0;
};

} // namespace ntype::client
