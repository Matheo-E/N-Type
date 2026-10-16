#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include <ntype/engine/ecs/Registry.hpp>
#include <ntype/rtype/protocol/Messages.hpp>

namespace ntype::server {

/// Turns the world into WorldSnapshot messages, split so that each one fits
/// in a datagram. Only entities already announced to clients are included.
class SnapshotBuilder {
public:
    [[nodiscard]] std::vector<rtype::protocol::WorldSnapshot>
    build(engine::ecs::Registry& registry, std::uint32_t tick,
          const std::unordered_set<std::uint32_t>& announced) const;

    [[nodiscard]] static rtype::protocol::SpawnedEntity
    spawn_message(engine::ecs::Registry& registry, engine::ecs::Entity entity);
};

} // namespace ntype::server
