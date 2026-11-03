#include "../include/System.hpp"
#include "../include/Factory.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr float Pi = 3.14159265f;
constexpr float FollowGap = 40.0f;  // followers stop this far from their parent

float SpeedOf(World& world, int id) {
    auto speed = world.Speeds.find(id);
    return speed != world.Speeds.end() ? speed->second.Value : 0.0f;
}

// 0 degrees is straight down; positive angles turn toward +x.
sf::Vector2f Direction(float degrees) {
    const float radians = degrees * Pi / 180.0f;
    return {std::sin(radians), std::cos(radians)};
}

// Flight patterns (ids used by "flight_pattern" in enemies.json).
sf::Vector2f FlightVelocity(int pattern, float time, float speed) {
    switch (pattern) {
        case 1: return {120.0f * std::cos(2.0f * time), speed};  // weaves down the screen
        case 2: return {speed * 3.0f * std::cos(time), 0.0f};     // sweeps side to side
        default: return {0.0f, 0.0f};
    }
}

void Shoot(World& world, int spawnerid, float degrees, float speed) {
    int bullet = CreateEBullet(world, spawnerid);
    world.Velocities[bullet].Xy = Direction(degrees) * speed;
}

// Attack patterns (ids used by "attack_pattern" in enemies.json).
// Pattern 0 is the player's shot: straight up, the bullet's default velocity.
void Fire(World& world, int spawnerid, int pattern) {
    switch (pattern) {
        case 0:
            CreateEBullet(world, spawnerid);
            break;
        case 1:  // a fan of three
            for (float angle : {-20.0f, 0.0f, 20.0f}) {
                Shoot(world, spawnerid, angle, 200.0f);
            }
            break;
        case 2:  // a ring of sixteen
            for (int i = 0; i < 16; ++i) {
                Shoot(world, spawnerid, 22.5f * i, 150.0f);
            }
            break;
        default:
            break;
    }
}

}  // namespace

void Flight(World& world, float dt) {
    for (auto& [id, flight] : world.Flights) {
        auto vel = world.Velocities.find(id);
        if (vel == world.Velocities.end()) {
            continue;
        }
        flight.Time += dt;
        vel->second.Xy = FlightVelocity(flight.PatternId, flight.Time, SpeedOf(world, id));
    }
}

void Follow(World& world) {
    for (auto& [id, parent] : world.Parents) {
        auto pos = world.Positions.find(id);
        auto vel = world.Velocities.find(id);
        if (pos == world.Positions.end() || vel == world.Velocities.end()) {
            continue;
        }
        vel->second.Xy = {0.0f, 0.0f};
        auto target = world.Positions.find(parent.ParentId);
        if (target == world.Positions.end()) {
            continue;
        }
        const sf::Vector2f delta = target->second.Xy - pos->second.Xy;
        const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (distance > FollowGap) {
            vel->second.Xy = delta / distance * SpeedOf(world, id);
        }
    }
}

// The player's spawner only counts up to its interval until Space is held, so
// the first shot is immediate. Bullets are created after the loop: creating
// entities while walking the spawners would be unsafe.
void Spawner(World& world, float dt) {
    const bool shooting = sf::Keyboard::isKeyPressed(sf::Keyboard::Space);
    std::vector<int> firing;
    for (auto& [id, spawner] : world.Spawners) {
        spawner.Timer += dt;
        if (world.Players.count(id) > 0 && !shooting) {
            spawner.Timer = std::min(spawner.Timer, spawner.Interval);
            continue;
        }
        if (spawner.Timer >= spawner.Interval) {
            spawner.Timer -= spawner.Interval;
            firing.push_back(id);
        }
    }
    for (int id : firing) {
        Fire(world, id, world.Spawners[id].PatternId);
    }
}
