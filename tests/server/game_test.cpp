#include "server/src/Game.hpp"

#include <cassert>
#include <iostream>
#include <string>

#include "Factory.hpp"
#include "System.hpp"

using ntype::server::Game;
using ntype::server::GameData;
using ntype::server::GameEvent;
using protocol::InputType;

namespace {

std::uint8_t keys(InputType a, InputType b)
{
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

bool at(const protocol::Entity& entity, std::uint16_t slot, std::int32_t x, std::int32_t y)
{
    return entity.id == slot && entity.kind == 1 && entity.x == x && entity.y == y;
}

constexpr float kDt = 1.0F / 60.0F;
constexpr int kChomper = 1; // enemies.json: 10 HP, 100 points

const GameData& real_data()
{
    static GameData data;
    static bool loaded = false;
    if (!loaded) {
        std::string error;
        loaded = ntype::server::load_game_data(data, NTYPE_ASSET_DIR, error);
        if (!loaded)
            std::cerr << error << '\n';
        assert(loaded);
    }
    return data;
}

// The enemy definitions, without any level: nothing spawns on its own.
GameData definitions_only()
{
    GameData data;
    data.definitions = real_data().definitions;
    return data;
}

int ship_entity(Game& game)
{
    assert(game.world().Players.size() == 1);
    return game.world().Players.begin()->first;
}

// Steps until an event of `kind` comes, for at most `seconds`.
bool step_until(Game& game, GameEvent::Kind kind, float seconds, GameEvent* found = nullptr)
{
    for (float time = 0; time < seconds; time += kDt) {
        game.step(kDt);
        for (const GameEvent& event : game.take_events()) {
            if (event.kind == kind) {
                if (found)
                    *found = event;
                return true;
            }
        }
    }
    return false;
}

void levels_load()
{
    const GameData& data = real_data();
    assert(data.levels.size() == 3);
    assert(data.levels[0].boss.enemy == 4 && data.levels[1].boss.enemy == 8 && data.levels[2].boss.enemy == 12);
    for (const auto& level : data.levels)
        assert(!level.waves.empty());
}

void waves_spawn()
{
    Game game(real_data());
    for (int tick = 0; tick < 60; ++tick)
        game.step(kDt);
    assert(game.world().Enemies.empty()); // the first wave enters at 2 s
    for (int tick = 0; tick < 120; ++tick)
        game.step(kDt);
    assert(!game.world().Enemies.empty());
}

// The shooter of the last bullet gets the enemy's points.
void kill_scores()
{
    const GameData data = definitions_only();
    Game game(data);
    game.add_ship(2); // centre (120, 290)
    CreateEEnemy(game.world(), {420, 290}, kChomper);
    game.set_input(2, static_cast<std::uint8_t>(InputType::Shoot));
    GameEvent scored;
    assert(step_until(game, GameEvent::Kind::Scored, 4.0F, &scored));
    assert(scored.slot == 2 && scored.points == 100 && game.world().Enemies.empty());
}

void player_dies()
{
    const GameData data = definitions_only();
    Game game(data);
    game.add_ship(3);
    game.world().Healths[ship_entity(game)].Hp = 1;
    CreateEEnemy(game.world(), game.world().Positions[ship_entity(game)].Xy, kChomper); // touching it
    GameEvent died;
    assert(step_until(game, GameEvent::Kind::PlayerDied, 0.5F, &died));
    assert(died.slot == 3 && !game.has_ship(3));
    for (const protocol::Entity& entity : game.snapshot())
        assert(entity.kind != 1); // only the enemy is left
}

void enemy_drops()
{
    const GameData data = definitions_only();
    Game game(data);
    game.add_ship(1);
    const int player = ship_entity(game);
    // Actual lethal collisions feed the server's drop logic and network snapshots.
    for (int kill = 0; kill <= 6; ++kill) {
        // Force each outcome, including "no drop", without a flaky random test.
        for (auto& [type, item] : game.world().ItemTypes) item.DropChance = type == kill ? 1.0f : 0.0f;
        CreateEEnemy(game.world(), {640, 360}, kChomper);
        const int bullet = CreateEBullet(game.world(), player);
        game.world().Positions[bullet].Xy = {640, 360};
        game.world().Damages[bullet].Dmg = 100;
        Collision(game.world());
        game.step(0);
        assert(game.world().Pickups.size() == static_cast<std::size_t>(kill));
    }
    bool seen[7] = {};
    for (const auto& entity : game.snapshot()) {
        if (entity.kind == static_cast<std::uint8_t>(protocol::EntityKind::Item)) {
            assert(entity.type >= 1 && entity.type <= 6);
            seen[entity.type] = true;
        }
    }
    for (int type = 1; type <= 6; ++type) assert(seen[type]);
    game.world().Positions[player].Xy = {640, 360};
    game.world().Healths[player].Hp = 1;
    game.step(0);
    assert(game.world().Pickups.empty());
    assert(game.world().Healths[player].Hp == 2);
    assert(game.world().Powerups[player].BurstTime == 10);
    assert(game.world().Invincibles.count(player) == 1);
    assert(game.snapshot().front().type == protocol::kGoldenPlayer);
    Timers(game.world(), 9.9f);
    assert(game.snapshot().front().type == protocol::kGoldenPlayer);
    Timers(game.world(), 0.2f);
    assert(game.snapshot().front().type == 0);
}

// Killing the boss clears the level; the next one starts clean, until the last.
void boss_clears_level()
{
    GameData data = definitions_only();
    ntype::server::Level level;
    level.boss = {0.0F, kChomper, 1, 1.0F, 420.0F, 170.0F, 0.0F};
    data.levels = {level, level};
    Game game(data);
    game.add_ship(1); // centre (120, 170), in line with the boss
    game.set_input(1, static_cast<std::uint8_t>(InputType::Shoot));
    assert(step_until(game, GameEvent::Kind::LevelCleared, 4.0F));
    assert(game.next_level() && game.level() == 1 && game.world().Bullets.empty());
    assert(step_until(game, GameEvent::Kind::LevelCleared, 4.0F));
    assert(!game.next_level() && game.level() == 1);
}

void ships()
{
    Game game;
    game.add_ship(1);
    game.add_ship(3);
    std::vector<protocol::Entity> ships = game.snapshot();
    assert(ships.size() == 2 && at(ships[0], 1, 100, 150) && at(ships[1], 3, 100, 390));

    // Ships move at 300 px/s and only with their own keys.
    game.set_input(1, static_cast<std::uint8_t>(InputType::Right));
    game.step(1.0F);
    ships = game.snapshot();
    assert(at(ships[0], 1, 400, 150) && at(ships[1], 3, 100, 390));

    // Positions stay inside the 1280x720 world (40x40 ships).
    game.set_input(1, keys(InputType::Up, InputType::Left));
    game.set_input(3, keys(InputType::Down, InputType::Right));
    game.step(10.0F);
    ships = game.snapshot();
    assert(at(ships[0], 1, 0, 0) && at(ships[1], 3, 1240, 680));

    game.remove_ship(1);
    assert(!game.has_ship(1) && game.has_ship(3));
    game.set_input(2, static_cast<std::uint8_t>(InputType::Right)); // no ship: ignored
    game.step(1.0F);
    ships = game.snapshot();
    assert(ships.size() == 1 && ships[0].id == 3);

    game.add_ship(3); // already there: unchanged
    assert(game.snapshot().size() == 1 && at(game.snapshot()[0], 3, 1240, 680));

}

void boss_reinforcements()
{
    GameData data = definitions_only();
    ntype::server::Level level;
    level.boss = {0, 4, 1, 1, 1000, 340, 0};
    level.support_enemies = {1, 2};
    level.support_interval = 7;
    level.support_max_alive = 2;
    data.levels = {level, level};
    // Keep reinforcements alive and on screen so the cap can be tested reliably.
    data.definitions.EnemyTypes[1].Speed = 0;
    data.definitions.EnemyTypes[2].Speed = 0;
    Game game(data);
    game.step(0);
    assert(game.world().Enemies.size() == 1);
    game.step(6);
    game.step(0);
    assert(game.world().Enemies.size() == 1);
    game.step(1);
    game.step(0);
    assert(game.world().Enemies.size() == 2);
    game.step(7);
    game.step(0);
    assert(game.world().Enemies.size() == 3);
    game.step(21);
    game.step(0);
    assert(game.world().Enemies.size() == 3); // cap, even after missed spawn times
    int reinforcement = -1;
    for (const auto& [id, enemy] : game.world().Enemies) if (enemy.TypeId == 1) reinforcement = id;
    assert(reinforcement >= 0);
    game.world().Destroy(reinforcement);
    game.step(7);
    game.step(0);
    assert(game.world().Enemies.size() == 3); // replenishes the vacancy
    int boss = -1;
    for (const auto& [id, enemy] : game.world().Enemies) if (enemy.TypeId == 4) boss = id;
    game.world().Destroy(boss);
    game.step(14);
    game.step(0);
    assert(game.world().Enemies.size() == 2); // no reinforcements without a living boss
    assert(game.next_level());
    game.step(0);
    assert(game.world().Enemies.size() == 1); // clean level; no immediate reinforcement
}

void zone_snapshots()
{
    const GameData data = definitions_only();
    Game game(data);
    game.add_ship(1);
    const int caster = CreateEEnemy(game.world(), {900, 360}, 3);
    game.world().Spawners[caster].Timer = game.world().Spawners[caster].Interval;
    game.step(0);
    const auto charge = [&]() {
        for (const auto& entity : game.snapshot())
            if (entity.kind == static_cast<std::uint8_t>(protocol::EntityKind::EnemyShot) && entity.type == 2)
                return static_cast<int>(entity.hp);
        return -1;
    };
    assert(charge() == 0);
    game.world().Positions[ship_entity(game)].Xy = {100, 650};
    Timers(game.world(), 0.75f);
    assert(charge() == 50);
    Timers(game.world(), 0.75f);
    assert(charge() == 100);
    Timers(game.world(), 2.1f);
    assert(charge() == -1); // expiry also reaches clients without killing the caster
    game.world().Spawners[caster].Timer = game.world().Spawners[caster].Interval;
    game.step(0);
    assert(charge() == 0);
    game.world().Destroy(caster);
    assert(charge() == -1); // removal reaches every client in the next snapshot
}

} // namespace

int main()
{
    ships();
    levels_load();
    waves_spawn();
    kill_scores();
    player_dies();
    enemy_drops();
    boss_clears_level();
    boss_reinforcements();
    zone_snapshots();
    std::cout << "Game tests passed!\n";
}
