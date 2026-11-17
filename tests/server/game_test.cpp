#include "server/src/Game.hpp"

#include <cassert>
#include <iostream>
#include <string>

#include "Factory.hpp"

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

} // namespace

int main()
{
    ships();
    levels_load();
    waves_spawn();
    kill_scores();
    player_dies();
    boss_clears_level();
    std::cout << "Game tests passed!\n";
}
