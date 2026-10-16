#include <gtest/gtest.h>

#include <ntype/engine/ecs/SystemScheduler.hpp>
#include <ntype/rtype/Components.hpp>
#include <ntype/rtype/EntityFactory.hpp>
#include <ntype/rtype/systems/Systems.hpp>

#include "../TestData.hpp"

using namespace ntype::rtype;
namespace systems = ntype::rtype::systems;

namespace {

struct World {
    ntype::engine::ecs::Registry registry;
    ntype::engine::events::EventBus events;
    ntype::engine::math::Random rng{1};
    EntityFactory factory{registry, test_game_data()};
    ntype::engine::ecs::SystemScheduler scheduler;
    std::vector<ntype::engine::events::Subscription> subscriptions;

    World() {
        const GameData& data = test_game_data();
        scheduler.add<systems::PlayerControlSystem>(data);
        scheduler.add<systems::WeaponSystem>(factory);
        scheduler.add<systems::MovementSystem>();
        scheduler.add<systems::BoundsSystem>(data.world_size);
        scheduler.add<systems::CollisionSystem>(events);
        scheduler.add<systems::DamageSystem>(registry, events, data);
        scheduler.add<systems::LifetimeSystem>();
        scheduler.add<systems::PlayerLifeSystem>(factory, events);
        scheduler.add<systems::CleanupSystem>(events);
    }

    void step(int ticks = 1) {
        for (int i = 0; i < ticks; ++i) {
            scheduler.update(registry, 1.0F / 60.0F);
        }
    }
};

} // namespace

TEST(Simulation, InputMovesThePlayerWithinTheScreen) {
    World world;
    const auto player = world.factory.create_player(0);
    const float start_x = world.registry.get<Transform>(player).position.x;
    world.registry.get<PlayerInput>(player).bits = input_bits::kRight;
    world.step(30);
    EXPECT_GT(world.registry.get<Transform>(player).position.x, start_x);
    world.registry.get<PlayerInput>(player).bits = input_bits::kUp;
    world.step(600);
    EXPECT_GE(world.registry.get<Transform>(player).position.y, 0.0F);
}

TEST(Simulation, PlayerMissileKillsEnemyAndScores) {
    World world;
    const auto player = world.factory.create_player(0);
    const auto player_position = world.registry.get<Transform>(player).position;
    const auto enemy_type = std::uint8_t{1}; // "pod": 1 hp, does not shoot
    const auto enemy =
        world.factory.create_enemy(enemy_type, player_position + Vec2f{200.0F, 0.0F}, world.rng);
    world.registry.get<Velocity>(enemy).linear = {};

    bool killed = false;
    world.subscriptions.push_back(
        world.events.subscribe<EnemyKilled>([&](const EnemyKilled& event) {
            killed = true;
            EXPECT_EQ(event.killer_slot, 0);
        }));
    world.registry.get<PlayerInput>(player).bits = input_bits::kFire;
    world.step(30);

    EXPECT_TRUE(killed);
    EXPECT_FALSE(world.registry.is_alive(enemy));
    EXPECT_EQ(world.registry.get<Score>(player).points, test_game_data().enemies[enemy_type].score);
}

TEST(Simulation, ContactCostsALifeAndThePlayerRespawns) {
    World world;
    const auto player = world.factory.create_player(0);
    world.registry.get<Player>(player).invulnerable = 0.0F;
    const auto position = world.registry.get<Transform>(player).position;
    static_cast<void>(world.factory.create_enemy(1, position, world.rng));

    world.step();
    const auto& state = world.registry.get<Player>(player);
    EXPECT_FALSE(state.alive);
    EXPECT_EQ(state.lives, test_game_data().player.lives - 1);

    world.step(static_cast<int>(test_game_data().player.respawn_delay * 60.0F) + 2);
    EXPECT_TRUE(world.registry.get<Player>(player).alive);
    EXPECT_GT(world.registry.get<Player>(player).invulnerable, 0.0F);
}

TEST(Simulation, ProjectilesExpireAndLeaveTheWorld) {
    World world;
    const auto player = world.factory.create_player(0);
    const auto missile = world.factory.create_missile(player, {1.0F, 0.0F});
    world.step(60 * 4);
    EXPECT_FALSE(world.registry.is_alive(missile));
}
