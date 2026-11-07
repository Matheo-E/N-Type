#include "../include/System.hpp"
#include "../include/Factory.hpp"
#include "../include/Math.hpp"

#include <algorithm>
#include <vector>

namespace {

constexpr float CullMargin = 64.0f;  // how far outside the window things survive
constexpr float FollowGap = 40.0f;   // followers stop this far from what they follow

float SpeedOf(World& world, int id) {
    auto speed = world.Speeds.find(id);
    return speed != world.Speeds.end() ? speed->second.Value : 0.0f;
}

// The velocity a flight pattern gives at a given time. toplayer is the vector from the
// entity to the player (used by Chase). See FlightPattern in PatternData.hpp.
sf::Vector2f FlightVelocity(const FlightPattern& pattern, const EFlight& flight, float speed, sf::Vector2f toplayer) {
    const float time = flight.Time;
    const float forward = speed * pattern.Forward;
    const sf::Vector2f along = Direction(pattern.DirectionDegrees + flight.Heading);
    const sf::Vector2f sideways = {-along.y, along.x};  // perpendicular to the direction
    switch (pattern.Type) {
        case FlightType::Stationary:
            return {0.0f, 0.0f};
        case FlightType::Straight:
            return along * forward;
        case FlightType::Sine:
            return along * forward + sideways * (pattern.Amplitude * std::cos(2.0f * Pi * pattern.Frequency * time));
        case FlightType::Path:  // handled in Flight: it needs the entity's position
            return {0.0f, 0.0f};
        case FlightType::Chase: {
            const float distance = Length(toplayer);
            return distance > 1.0f ? toplayer / distance * forward : sf::Vector2f(0.0f, 0.0f);
        }
    }
    return {0.0f, 0.0f};
}

// Where one step is at progress u (0 to 1), given the point it starts from. All positions
// are relative to the spawn point. See PathStep in PatternData.hpp.
sf::Vector2f StepPosition(const PathStep& step, sf::Vector2f from, float u) {
    switch (step.Kind) {
        case StepKind::Wait:
            return from;
        case StepKind::Move:
            return from + (step.To - from) * u;
        case StepKind::Curve: {  // cubic Bezier
            const float v = 1.0f - u;
            return from * (v * v * v) + step.Control1 * (3.0f * v * v * u) + step.Control2 * (3.0f * v * u * u) + step.To * (u * u * u);
        }
        case StepKind::Arc: {  // the circle's centre is chosen so that the arc starts at `from`
            const sf::Vector2f centre = from - Direction(step.StartDegrees) * step.Radius;
            return centre + Direction(step.StartDegrees + step.SweepDegrees * u) * step.Radius;
        }
    }
    return from;
}

// Where a path puts the entity after `time` seconds, relative to the spawn point.
// Walks the steps in order; after the last one the entity stays at its end.
sf::Vector2f PathPosition(const FlightPattern& pattern, float time) {
    sf::Vector2f from = {0.0f, 0.0f};
    for (const PathStep& step : pattern.Steps) {
        if (time < step.Duration) {
            return StepPosition(step, from, time / step.Duration);
        }
        time -= step.Duration;
        from = StepPosition(step, from, 1.0f);
    }
    return from;
}

// The angle of bullet number i in a volley whose middle is at angle base.
//  - a ring (spread of 360 or more): bullets evenly around, so the first and last do not overlap
//  - a fan: bullets evenly across the spread, centred on base
//  - one bullet: straight along base
float BulletAngle(const AttackPattern& pattern, float base, int i) {
    if (pattern.Count <= 1) {
        return base;
    }
    if (pattern.SpreadDegrees >= 360.0f) {
        return base + pattern.SpreadDegrees / pattern.Count * i;
    }
    return base - pattern.SpreadDegrees / 2.0f + pattern.SpreadDegrees / (pattern.Count - 1) * i;
}

// Fires one volley from spawnerid. Aimed volleys are centred on the player.
void Fire(World& world, int spawnerid, const AttackPattern& pattern, int playerid) {
    const sf::Vector2f from = world.Positions[spawnerid].Xy;
    auto player = world.Positions.find(playerid);
    float base = pattern.AngleDegrees;
    if (pattern.Aimed && player != world.Positions.end()) {
        base = AngleTo(from, player->second.Xy);
    }
    for (int i = 0; i < pattern.Count; ++i) {
        const int bullet = CreateEBullet(world, spawnerid);
        if (bullet < 0) {
            return;
        }
        const float angle = BulletAngle(pattern, base, i);
        world.Velocities[bullet].Xy = Direction(angle) * pattern.Speed;
        auto flight = world.Flights.find(bullet);  // a bullet with a flight pattern turns it to its own angle
        if (flight != world.Flights.end()) {
            flight->second.Heading = angle;
        }
    }
}

}  // namespace

// Each entity with a flight pattern gets a new velocity every frame, from its pattern,
// its own speed, and how long it has been flying. Movement then moves it.
void Flight(World& world, float dt, int playerid) {
    auto player = world.Positions.find(playerid);
    for (auto& [id, flight] : world.Flights) {
        auto pattern = world.FlightPatterns.find(flight.PatternId);
        auto pos = world.Positions.find(id);
        auto vel = world.Velocities.find(id);
        if (pattern == world.FlightPatterns.end() || pos == world.Positions.end() || vel == world.Velocities.end()) {
            continue;
        }
        flight.Time += dt;
        if (pattern->second.Type == FlightType::Path) {
            // A path says where to be, not how fast to go: pick the velocity that gets there
            // this frame, so Movement still moves the entity and it cannot drift off the path.
            const sf::Vector2f target = flight.Origin + Rotate(PathPosition(pattern->second, flight.Time), flight.Heading);
            vel->second.Xy = dt > 0.0f ? (target - pos->second.Xy) / dt : sf::Vector2f(0.0f, 0.0f);
            continue;
        }
        const sf::Vector2f toplayer = player != world.Positions.end() ? player->second.Xy - pos->second.Xy : sf::Vector2f(0, 0);
        vel->second.Xy = FlightVelocity(pattern->second, flight, SpeedOf(world, id), toplayer);
    }
}

// Each entity with an EParent heads for its parent at its own speed (ESpeed) and stops
// FollowGap pixels away from it, without overshooting. A chain of followers (each one
// following the one in front) makes a centipede. If the parent is gone, it stops.
void Follow(World& world, float dt) {
    for (auto& [id, parent] : world.Parents) {
        auto pos = world.Positions.find(id);
        auto vel = world.Velocities.find(id);
        if (pos == world.Positions.end() || vel == world.Velocities.end()) {
            continue;
        }
        vel->second.Xy = {0.0f, 0.0f};
        auto target = world.Positions.find(parent.ParentId);
        if (target == world.Positions.end() || dt <= 0.0f) {
            continue;
        }
        const sf::Vector2f delta = target->second.Xy - pos->second.Xy;
        const float distance = Length(delta);
        if (distance > FollowGap) {
            const float speed = std::min(SpeedOf(world, id), (distance - FollowGap) / dt);
            vel->second.Xy = delta / distance * speed;
        }
    }
}

// Each spawner counts up; when it reaches its interval it fires its attack pattern.
// The player's spawner only fires while the Shoot action is held (and is kept ready,
// so the first shot is immediate). Bullets are created after the loop, because creating
// entities while walking the spawners would not be safe.
void Spawner(World& world, float dt, int playerid) {
    const bool shooting = world.Actions.IsDown(Action::Shoot);
    std::vector<int> firing;
    for (auto& [id, spawner] : world.Spawners) {
        spawner.Timer += dt;
        if (spawner.Interval <= 0.0f) {
            continue;
        }
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
        auto pattern = world.AttackPatterns.find(world.Spawners[id].PatternId);
        if (pattern != world.AttackPatterns.end()) {
            Fire(world, id, pattern->second, playerid);
        }
    }
}

// Counts the lifetimes down and removes what has expired. Also ends invincibility.
void Timers(World& world, float dt) {
    std::vector<int> expired;
    for (auto& [id, life] : world.Lifetimes) {
        life.Lifetime -= dt;
        if (life.Lifetime <= 0.0f) {
            expired.push_back(id);
        }
    }
    for (auto it = world.Invincibles.begin(); it != world.Invincibles.end();) {
        it->second.Time -= dt;
        it = it->second.Time <= 0.0f ? world.Invincibles.erase(it) : std::next(it);
    }
    for (int id : expired) {
        world.Destroy(id);
    }
}

// Removes bullets, enemies and items that are far outside the window, so they do not pile up.
void Cull(World& world) {
    std::vector<int> gone;
    for (auto& [id, pos] : world.Positions) {
        if (world.Bullets.count(id) == 0 && world.Enemies.count(id) == 0 && world.Pickups.count(id) == 0) {
            continue;
        }
        const sf::Vector2f p = pos.Xy;
        if (p.x < -CullMargin || p.x > WindowWidth + CullMargin || p.y < -CullMargin || p.y > WindowHeight + CullMargin) {
            gone.push_back(id);
        }
    }
    for (int id : gone) {
        world.Destroy(id);
    }
}
