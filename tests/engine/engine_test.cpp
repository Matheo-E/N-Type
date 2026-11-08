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
        const int bullet = CreateEBullet(world, player); // 2 damage each
        world.Positions[bullet].Xy = where;
        Collision(world);
    }
    assert(world.Healths[chomper].Hits == 3 && world.Healths[chomper].Hp == 4);
}

void items()
{
    World world = loaded();
    assert(world.ItemTypes.size() == 6);
    const int player = CreateEPlayer(world, {100, 360});
    const int other = CreateEPlayer(world, {100, 360});
    // A shared pickup belongs to exactly one of the overlapping players.
    const int shared = CreateEItem(world, {100, 360}, 5);
    Pickup(world);
    assert(world.Pickups.count(shared) == 0);
    assert(world.Powerups.size() == 1);
    world.Destroy(other);
    world.Powerups.clear();

    for (int item = 1; item <= 6; ++item) {
        world.Healths[player].Hp = 1;
        const int pickup = CreateEItem(world, {100, 360}, item);
        assert(pickup >= 0);
        Pickup(world);
        assert(world.Pickups.count(pickup) == 0);
        if (item <= 3) {
            world.Players[player].Shoot = true;
            world.Spawners[player].Timer = world.Spawners[player].Interval;
            const int before = bullets_of(world, player);
            Spawner(world, 0);
            assert(bullets_of(world, player) - before == item + 1);
            for (const auto& [id, owner] : world.OwnerIds) {
                if (owner.OId == player) assert(world.Velocities[id].Xy.x > 0);
            }
        } else if (item == 4) {
            assert(world.Healths[player].Hp == 2);
            ApplyItem(world, player, item);
            ApplyItem(world, player, item);
            assert(world.Healths[player].Hp == 3); // capped at maximum health
        } else if (item == 5) {
            assert(world.Powerups[player].BurstTime == 10);
            assert(world.Damages[CreateEBullet(world, player)].Dmg == 2);
        } else {
            // Protection blocks contact damage for ten seconds.
            std::vector<int> bullets;
            for (const auto& [id, bullet] : world.Bullets) bullets.push_back(id);
            for (int id : bullets) world.Destroy(id);
            CreateEEnemy(world, {100, 360}, kChomper);
            Collision(world);
            assert(world.Healths[player].Hp == 1);
            Timers(world, 9.9f);
            Collision(world);
            assert(world.Healths[player].Hp == 1 && world.Powerups[player].GoldenTime > 0);
            Timers(world, 0.2f);
            assert(world.Powerups.empty());
            Collision(world);
            assert(world.Players.count(player) == 0);
        }
    }
    const int expired = CreateEItem(world, {640, 360}, 1);
    Flight(world, 1);
    assert(world.Velocities[expired].Xy.x < 0);
    const int short_lived = CreateEItem(world, {640, 360}, 6);
    Timers(world, 8.1f);
    assert(world.Pickups.count(short_lived) == 0 && world.Pickups.count(expired) == 1);
    Timers(world, 12.0f);
    assert(world.Pickups.count(expired) == 0);
}

void burst_fire()
{
    World world = loaded();
    const int player = CreateEPlayer(world, {100, 360});
    world.Players[player].Shoot = true;
    for (int i = 0; i < 100; ++i) Spawner(world, 0.01f);
    assert(bullets_of(world, player) == 2); // normal cadence reduced from ~6.7/s to ~2.9/s
    const float normal_interval = world.Spawners[player].Interval;
    ApplyItem(world, player, 5);
    int before = bullets_of(world, player);
    Spawner(world, 0);
    assert(bullets_of(world, player) == before + 1);
    Spawner(world, 0.1f);
    Spawner(world, 0.1f);
    assert(bullets_of(world, player) == before + 3);
    Spawner(world, 0.5f);
    assert(bullets_of(world, player) == before + 3); // pause between bursts
    Spawner(world, 0.11f);
    assert(bullets_of(world, player) == before + 4);
    Timers(world, 9.9f);
    ApplyItem(world, player, 5); // refresh rather than stacking durations
    assert(world.Powerups[player].BurstTime == 10);
    Timers(world, 10.1f);
    assert(world.Powerups.empty() && world.Spawners[player].Interval == normal_interval);
    world.Spawners[player].Timer = 0;
    before = bullets_of(world, player);
    Spawner(world, 0.1f);
    assert(bullets_of(world, player) == before);
    Spawner(world, 0.26f);
    assert(bullets_of(world, player) == before + 1);
}

} // namespace

int main()
{
    per_player_input();
    nearest_player();
    deaths();
    hits_counted();
    items();
    burst_fire();
    std::cout << "Engine tests passed!\n";
}
