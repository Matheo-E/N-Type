#include "server/src/Game.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
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
    data.zones = real_data().zones;
    data.origins = real_data().origins;
    data.beams = real_data().beams;
    data.beam_casters = real_data().beam_casters;
    data.items = real_data().items;
    return data;
}

int bullets_of(const World& world, int shooter)
{
    int count = 0;
    for (const auto& [id, owner] : world.OwnerIds)
        count += world.Bullets.count(id) > 0 && owner.OId == shooter ? 1 : 0;
    return count;
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
    GameData data = definitions_only();
    Game game(data);
    game.add_ship(1);
    const int player = ship_entity(game);
    // Actual lethal collisions feed the server's drop logic and network snapshots.
    for (int kill = 0; kill <= 6; ++kill) {
        // Force each outcome, including "no drop", without a flaky random test.
        for (auto& [type, rules] : data.items) rules.drop_chance = type == kill ? 1.0f : 0.0f;
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
    assert(game.burst_time(1) == 10);
    assert(game.world().Invincibles.count(player) == 1);
    assert(game.snapshot().front().type == protocol::kGoldenPlayer);
    game.step(9.9f);
    assert(game.snapshot().front().type == protocol::kGoldenPlayer);
    game.step(0.2f);
    assert(game.snapshot().front().type == 0 && game.golden_time(1) == 0);
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
    game.step(0.75f);
    assert(charge() == 50);
    game.step(0.75f);
    assert(charge() == 100);
    game.step(2.1f);
    assert(charge() == -1); // expiry also reaches clients without killing the caster
    game.step(0.4f); // the caster's next cast, 4 s after the first
    assert(charge() >= 0 && charge() < 100);
    game.world().Destroy(caster);
    assert(charge() == -1); // removal reaches every client in the next snapshot
}

// Shots `shooter` fired since the world's NextId was `since`.
int fired_since(const World& world, int shooter, int since)
{
    int count = 0;
    for (const auto& [id, owner] : world.OwnerIds)
        count += id >= since && owner.OId == shooter ? 1 : 0;
    return count;
}

bool close(float a, float b)
{
    return std::fabs(a - b) < 0.5f;
}

// Bosses fire from several points ("origins" in attack_patterns.json), each volley aimed from
// its own point; the old cadence no longer fires a second volley.
void boss_attacks()
{
    const GameData data = definitions_only();
    Game game(data);
    game.add_ship(1);
    World& world = game.world();
    const int ice = CreateEEnemy(world, {1000, 340}, 4);
    world.Spawners[ice].Timer = world.Spawners[ice].Interval; // ready
    game.step(0);
    assert(bullets_of(world, ice) == 16);
    int upper = 0, lower = 0;
    for (const auto& [id, owner] : world.OwnerIds) {
        if (owner.OId != ice) continue;
        const auto position = world.Positions[id].Xy;
        assert(close(position.x, 820) && world.Velocities[id].Xy.x < 0);
        upper += close(position.y, 170);
        lower += close(position.y, 510);
    }
    assert(upper == 8 && lower == 8);
    int next = world.NextId;
    game.step(1.8f);
    assert(fired_since(world, ice, next) == 0); // no second volley yet
    const int bear = CreateEEnemy(world, {1000, 350}, 8);
    world.Spawners[bear].Timer = world.Spawners[bear].Interval;
    game.step(0);
    assert(bullets_of(world, bear) == 9);
    next = world.NextId;
    game.step(1.4f);
    assert(fired_since(world, bear, next) == 0); // no second volley yet

    const int carry = CreateEEnemy(world, {1100, 360}, 12);
    world.Spawners[carry].Timer = world.Spawners[carry].Interval;
    game.step(0);
    assert(bullets_of(world, carry) == 4);
    for (const auto& [id, owner] : world.OwnerIds) {
        if (owner.OId == carry) assert(world.Velocities[id].Xy.x < 0 && world.Damages[id].Dmg == 2);
    }
    next = world.NextId;
    game.step(0.45f);
    assert(fired_since(world, carry, next) == 0); // no second volley yet
    // The patrol, measured from a fresh start as when it enters.
    world.Positions[carry].Xy = {1100, 360};
    world.Flights[carry].Time = 0;
    float low = 720, high = 0;
    for (int tick = 0; tick < 240; ++tick) {
        game.step(1.f / 60);
        const auto position = world.Positions[carry].Xy;
        assert(close(position.x, 1100) && position.y > 90 && position.y < 630);
        low = std::min(low, position.y);
        high = std::max(high, position.y);
    }
    assert(low < 125 && high > 595); // crosses almost the whole height in under four seconds
}

// The first boss's beam: one entity across the screen at y = 360, every 8 s; harmless while it
// warns (1.3 s), then 2 damage to each player inside it, once; gone after 1.8 s or with the boss.
void ice_beam()
{
    GameData data = definitions_only();
    assert(data.beam_casters.at(4) == 10 && data.beams.at(10).length == 1280);
    Game game(data);
    game.add_ship(1);
    game.add_ship(2);
    World& world = game.world();
    int inside = -1, outside = -1;
    for (const auto& [id, player] : world.Players) (inside < 0 ? inside : outside) = id;
    const int boss = CreateEEnemy(world, {1100, 360}, 4);
    world.Spawners[boss].Interval = 0; // only the beam, for this test
    const auto beam_hp = [&]() {
        for (const auto& entity : game.snapshot())
            if (entity.kind == static_cast<std::uint8_t>(protocol::EntityKind::EnemyShot) && entity.type == 10) {
                assert(entity.x == 640 && entity.y == 360); // its centre: the strip spans the screen
                return static_cast<int>(entity.hp);
            }
        return -1;
    };
    const auto place = [&]() { // the game keeps ships inside the screen; put them back each step
        world.Positions[inside].Xy = {300, 380};  // 20 px off the middle line: inside the 96 px strip
        world.Positions[outside].Xy = {300, 430}; // 70 px off: outside
    };
    place();
    game.step(7.9f);
    assert(beam_hp() == -1); // not yet
    place();
    game.step(0.1f);
    assert(beam_hp() >= 0 && beam_hp() < 100); // warning
    for (const auto& [id, player] : world.Players) assert(world.Healths[id].Hp == 3);
    place();
    game.step(0.55f);
    assert(beam_hp() == 50); // half of the 1.3 s warning (the 0.1 s step that cast it counts)
    place();
    game.step(0.7f); // the warning ends
    assert(beam_hp() == 100);
    place();
    game.step(0);
    assert(world.Healths[inside].Hp == 1 && world.Healths[outside].Hp == 3);
    world.Invincibles.clear();
    place();
    game.step(1.0f);
    place();
    game.step(0);
    assert(world.Healths[inside].Hp == 1); // once per beam
    place();
    game.step(1.0f);
    assert(beam_hp() == -1); // 1.3 s + 1.8 s, then gone
    world.Healths[inside].Hp = 3;

    // The next one, 8 s after the first, goes with its boss.
    place();
    game.step(4.6f);
    place();
    game.step(0.2f);
    assert(beam_hp() >= 0);
    world.Destroy(boss);
    assert(beam_hp() == -1);
    game.step(0);
    for (const auto& [id, player] : world.Players) assert(world.Healths[id].Hp == 3);
}

// Burst fire (dental floss, item 5): three volleys 0.1 s apart, then a 0.6 s pause.
void burst_fire()
{
    const GameData data = definitions_only();
    Game game(data);
    game.add_ship(1);
    const int player = ship_entity(game);
    game.set_input(1, static_cast<std::uint8_t>(InputType::Shoot));
    for (int i = 0; i < 100; ++i) game.step(0.01f);
    assert(bullets_of(game.world(), player) == 2); // the ship's normal cadence: about three per second
    const float normal_interval = game.world().Spawners[player].Interval;
    CreateEItem(game.world(), game.world().Positions[player].Xy, 5);
    game.step(0); // picked up
    assert(game.burst_time(1) == 10);
    int before = bullets_of(game.world(), player);
    game.step(0);
    assert(bullets_of(game.world(), player) == before + 1);
    game.step(0.1f);
    game.step(0.1f);
    assert(bullets_of(game.world(), player) == before + 3);
    game.step(0.5f);
    assert(bullets_of(game.world(), player) == before + 3); // pause between bursts
    game.step(0.11f);
    assert(bullets_of(game.world(), player) == before + 4);
    game.step(9.0f);
    CreateEItem(game.world(), game.world().Positions[player].Xy, 5);
    game.step(0);
    assert(game.burst_time(1) == 10); // refreshed rather than stacked
    game.step(10.1f);
    assert(game.burst_time(1) == 0 && game.world().Spawners[player].Interval == normal_interval);
    game.world().Spawners[player].Timer = 0;
    before = bullets_of(game.world(), player);
    game.step(0.1f);
    assert(bullets_of(game.world(), player) == before);
    game.step(0.26f);
    assert(bullets_of(game.world(), player) == before + 1); // back to the normal cadence
}

// Target zones: placed on the nearest player, harmless while warning, then hurting every
// player inside at most every two seconds; bounded in number; gone with their caster.
void targeted_zones()
{
    const GameData data = definitions_only();
    Game game(data);
    game.add_ship(1);
    game.add_ship(2);
    game.add_ship(3);
    int ids[3];
    {
        int n = 0;
        for (const auto& [id, player] : game.world().Players) ids[n++] = id;
    }
    for (int id : ids) game.world().Positions[id].Xy = {100, 360};
    const int player = ids[0], escaped = ids[1], stayed = ids[2];
    const int caster = CreateEEnemy(game.world(), {900, 360}, 3);
    game.world().Spawners[caster].Timer = game.world().Spawners[caster].Interval; // ready
    game.step(0);
    assert(game.zone_count() == 1 && bullets_of(game.world(), caster) == 1);
    int zone = -1;
    for (const auto& [id, owner] : game.world().OwnerIds) if (owner.OId == caster) zone = id;
    assert(game.world().Positions[zone].Xy == sf::Vector2f(100, 360));
    assert(std::fabs(game.world().Lifetimes[zone].Lifetime - 3.5f) < 0.01f); // 1.5 s warning + 2 s
    assert(game.world().Healths[player].Hp == 3); // warning is harmless
    game.world().Positions[escaped].Xy = {400, 100};
    game.step(1.4f);
    assert(game.world().Healths[player].Hp == 3);
    game.step(0.2f); // the warning ends
    game.step(0);
    assert(game.world().Healths[player].Hp == 2 && game.world().Healths[escaped].Hp == 3);
    assert(game.world().Healths[stayed].Hp == 2); // a zone can hit several players
    assert(game.zone_count() == 1);               // and is not used up
    game.step(1.1f);
    assert(game.world().Healths[player].Hp == 2); // two seconds before it can hurt them again
    game.world().Positions[player].Xy = {400, 100};
    game.world().Positions[stayed].Xy = {400, 100};
    game.step(0.9f);
    assert(game.zone_count() == 0); // 1.5 s warning + 2 s active, then gone

    // Bounded: two per caster, three in all. Long-lived zones and protected ships, so that
    // nothing expires or dies while counting.
    GameData lasting = definitions_only();
    lasting.invincible = true;
    for (int pattern : {2, 5}) lasting.definitions.AttackPatterns[pattern].Lifetime = 1000;
    Game bounded(lasting);
    bounded.add_ship(1);
    const int first = CreateEEnemy(bounded.world(), {900, 360}, 3); // a cast every 4 s
    bounded.world().Spawners[first].Timer = bounded.world().Spawners[first].Interval;
    bounded.step(0);
    assert(bounded.zone_count() == 1);
    bounded.step(4);
    assert(bounded.zone_count() == 2);
    bounded.step(4);
    assert(bounded.zone_count() == 2); // two per caster
    const int second = CreateEEnemy(bounded.world(), {950, 500}, 7);
    bounded.world().Spawners[second].Timer = bounded.world().Spawners[second].Interval;
    bounded.step(0);
    assert(bounded.zone_count() == 3);
    bounded.step(6);
    assert(bounded.zone_count() == 3); // three in all

    // Leaving the screen cannot take a caster's zones away: it stops at x = 200.
    bounded.step(100);
    assert(std::fabs(bounded.world().Positions[first].Xy.x - 200) < 0.5f && bounded.zone_count() == 3);
    const int shooter = bounded.world().Players.begin()->first;
    const int bullet = CreateEBullet(bounded.world(), shooter);
    bounded.world().Positions[bullet].Xy = bounded.world().Positions[first].Xy;
    bounded.world().Damages[bullet].Dmg = 100;
    bounded.step(0); // the caster dies: its zones go with it
    assert(bounded.world().Enemies.count(first) == 0 && bounded.zone_count() == 1);
    for (const auto& [id, owner] : bounded.world().OwnerIds)
        if (bounded.world().Bullets.count(id) && owner.OId != shooter) assert(owner.OId == second);
    bounded.world().Destroy(second);
    bounded.step(0);
    assert(bounded.zone_count() == 0);
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
    burst_fire();
    targeted_zones();
    boss_attacks();
    ice_beam();
    std::cout << "Game tests passed!\n";
}
