// Engine behaviour with several players, on the real data in assets/data.
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

#include "Factory.hpp"
#include "Loader.hpp"
#include "Math.hpp"
#include "System.hpp"

namespace {

const std::string kData = std::string(NTYPE_ASSET_DIR) + "/data/";
constexpr int kChomper = 1; // enemies.json: chases the player
constexpr int kSmice = 2;   // enemies.json: aimed shot

World loaded()
{
    World world;
    assert(LoadAttackPatterns(world, kData + "attack_patterns.json"));
    assert(LoadFlightPatterns(world, kData + "flight_patterns.json"));
    assert(LoadEnemies(world, kData + "enemies.json"));
    assert(LoadItems(world, kData + "items.json"));
    return world;
}

int bullets_of(const World& world, int shooter)
{
    int count = 0;
    for (const auto& [id, owner] : world.OwnerIds)
        count += world.Bullets.count(id) > 0 && owner.OId == shooter ? 1 : 0;
    return count;
}

bool close(float a, float b)
{
    return std::fabs(a - b) < 0.5f;
}

// Each player moves and shoots from its own EPlayer, not from a shared keyboard.
void per_player_input()
{
    World world = loaded();
    const int first = CreateEPlayer(world, {100, 100});
    const int second = CreateEPlayer(world, {100, 300});
    Spawner(world, 1.0f); // idle guns charge up to one interval: the first shot is then immediate
    world.Players[first].Move = {1, 1};
    world.Players[first].Shoot = true;
    MoveInput(world);
    Spawner(world, 0.016f);
    const sf::Vector2f velocity = world.Velocities[first].Xy;
    assert(close(Length(velocity), world.Speeds[first].Value) && velocity.x > 0 && velocity.y > 0);
    assert(world.Velocities[second].Xy == sf::Vector2f(0, 0));
    assert(bullets_of(world, first) == 1 && bullets_of(world, second) == 0);
}

// Chasers and aimed shots go for the nearest player.
void nearest_player()
{
    World world = loaded();
    CreateEPlayer(world, {100, 360});        // far
    const int near = CreateEPlayer(world, {900, 100});

    const int chomper = CreateEEnemy(world, {600, 360}, kChomper);
    Flight(world, 0.016f);
    const sf::Vector2f chase = world.Velocities[chomper].Xy;
    assert(chase.x > 0 && chase.y < 0); // up and right, towards (900, 100)

    const sf::Vector2f from = {1000, 360};
    const int smice = CreateEEnemy(world, from, kSmice);
    world.Spawners[smice].Timer = world.Spawners[smice].Interval; // ready to fire
    Spawner(world, 0.0f);
    assert(bullets_of(world, smice) == 1);
    for (const auto& [id, owner] : world.OwnerIds) {
        if (owner.OId != smice)
            continue;
        const sf::Vector2f shot = world.Velocities[id].Xy;
        assert(close(AngleTo({0, 0}, shot), AngleTo(from, world.Positions[near].Xy)));
    }
}

// A kill is noted once, with the shooter, the enemy type and where it died.
void deaths()
{
    World world = loaded();
    const int player = CreateEPlayer(world, {100, 360});
    const sf::Vector2f where = {640, 360};
    const int chomper = CreateEEnemy(world, where, kChomper);
    for (int shot = 0; shot < 2; ++shot) { // two lethal bullets in the same frame
        const int bullet = CreateEBullet(world, player);
        world.Positions[bullet].Xy = where;
        world.Damages[bullet].Dmg = 100;
    }
    Collision(world);
    assert(world.Deaths.size() == 1);
    assert(world.Healths.count(player) == 1 && world.Healths[player].Hits == 0);
    const EDeath& death = world.Deaths.front();
    assert(death.Id == chomper && death.Killer == player && death.EnemyType == kChomper);
    assert(!death.Player && death.Position == where);
    assert(world.Enemies.count(chomper) == 0 && world.Bullets.empty());
}

// Every hit that hurts is counted, so a game can show each impact.
void hits_counted()
{
    World world = loaded();
    const int player = CreateEPlayer(world, {100, 360});
    const sf::Vector2f where = {640, 360};
    const int chomper = CreateEEnemy(world, where, kChomper); // 10 HP
    for (int shot = 0; shot < 3; ++shot) {
        const int bullet = CreateEBullet(world, player); // 1 damage each
        world.Positions[bullet].Xy = where;
        Collision(world);
    }
    assert(world.Healths[chomper].Hits == 3 && world.Healths[chomper].Hp == 7);
}

} // namespace

int main()
{
    per_player_input();
    nearest_player();
    deaths();
    hits_counted();
    std::cout << "Engine tests passed!\n";
}
