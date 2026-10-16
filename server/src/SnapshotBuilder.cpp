#include "SnapshotBuilder.hpp"

#include <algorithm>

#include <ntype/rtype/Components.hpp>

namespace ntype::server {

namespace protocol = rtype::protocol;
using namespace rtype;
using engine::ecs::Entity;

std::vector<protocol::WorldSnapshot>
SnapshotBuilder::build(engine::ecs::Registry& registry, const std::uint32_t tick,
                       const std::unordered_set<std::uint32_t>& announced) const {
    std::vector<protocol::PlayerState> players;
    registry.view<Player, Score>().each([&](Entity, const Player& player, const Score& score) {
        players.push_back({player.slot, player.lives, score.points, player.alive});
    });

    std::vector<protocol::EntityState> states;
    registry.view<NetworkId, Transform>().each(
        [&](const Entity entity, const NetworkId& network_id, const Transform& transform) {
            if (!announced.contains(network_id.id)) {
                return;
            }
            protocol::EntityState state;
            state.network_id = network_id.id;
            state.x = protocol::quantize_position(transform.position.x);
            state.y = protocol::quantize_position(transform.position.y);
            if (const auto* velocity = registry.try_get<Velocity>(entity)) {
                state.vx = protocol::quantize_velocity(velocity->linear.x);
                state.vy = protocol::quantize_velocity(velocity->linear.y);
            }
            if (const auto* health = registry.try_get<Health>(entity)) {
                state.hp = static_cast<std::uint8_t>(std::clamp(health->current, 0, 255));
            }
            if (const auto* player = registry.try_get<Player>(entity)) {
                if (!player->alive) {
                    state.flags |= protocol::entity_flags::kHidden;
                }
                if (player->invulnerable > 0.0F) {
                    state.flags |= protocol::entity_flags::kInvulnerable;
                }
            }
            states.push_back(state);
        });

    std::vector<protocol::WorldSnapshot> snapshots;
    std::size_t offset = 0;
    do {
        const std::size_t count =
            std::min(protocol::kMaxEntitiesPerSnapshot, states.size() - offset);
        protocol::WorldSnapshot snapshot;
        snapshot.server_tick = tick;
        snapshot.players = players;
        snapshot.entities.assign(states.begin() + static_cast<std::ptrdiff_t>(offset),
                                 states.begin() + static_cast<std::ptrdiff_t>(offset + count));
        snapshots.push_back(std::move(snapshot));
        offset += count;
    } while (offset < states.size());
    return snapshots;
}

protocol::SpawnedEntity SnapshotBuilder::spawn_message(engine::ecs::Registry& registry,
                                                       const engine::ecs::Entity entity) {
    const auto& type = registry.get<EntityType>(entity);
    const auto& position = registry.get<Transform>(entity).position;
    return {registry.get<NetworkId>(entity).id, type.kind, type.subtype,
            protocol::quantize_position(position.x), protocol::quantize_position(position.y)};
}

} // namespace ntype::server
