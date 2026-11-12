#include "Game.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "Factory.hpp"
#include "Math.hpp"
#include "GameConfig.hpp"
#include "System.hpp"

namespace ntype::server {

namespace {

using protocol::InputType;

// Ships are 40x40 boxes; the engine places entities by their centre.
constexpr float kHalf = config::kPlayerSize / 2.0F;
constexpr float kMinCentre = kHalf;
constexpr float kMaxCentreX = static_cast<float>(config::kWidth) - kHalf;
constexpr float kMaxCentreY = static_cast<float>(config::kHeight) - kHalf;

bool pressed(const std::uint8_t keys, const InputType key) {
    return (keys & static_cast<std::uint8_t>(key)) != 0;
}

// Ids 1 to 4 are the players' slots; engine ids map to 5 and up.
std::uint16_t network_id(const int entity) {
    return static_cast<std::uint16_t>(config::kMaxPlayers + 1 + static_cast<unsigned>(entity) % (65536U - 5U));
}

std::uint8_t small(const int value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

std::int32_t rounded(const float value) {
    return static_cast<std::int32_t>(std::lround(value));
}

constexpr float kShipFireInterval = 0.35F; // seconds between two volleys: about three per second

// Burst fire (ItemRules::burst_duration): three volleys 0.1 s apart, then a 0.6 s pause.
constexpr float kBurstGap = 0.1F;
constexpr float kBurstPause = 0.6F;
constexpr int kBurstVolleys = 3;

// Target zones: at most three on the ground, two per caster; each hurts a given player at
// most every two seconds. Casters stop at x = 200 so the players can still reach them.
constexpr std::size_t kMaxZones = 3;
constexpr int kMaxZonesPerCaster = 2;
constexpr float kZoneHitCooldown = 2.0F;
constexpr float kCasterStopX = 200.0F;

// The engine's spawner fires `id` this frame (its rule, in Behaviour.cpp): a set interval,
// a player only while shooting, and the timer reaching the interval.
bool fires_now(const World& world, const int id, const ESpawner& spawner, const float dt) {
    if (spawner.Interval <= 0.0F) {
        return false;
    }
    const auto player = world.Players.find(id);
    if (player != world.Players.end() && !player->second.Shoot) {
        return false;
    }
    return spawner.Timer + dt >= spawner.Interval;
}

// Bullet number i of a volley centred on `base`: the engine's rule (BulletAngle in Behaviour.cpp).
float bullet_angle(const AttackPattern& pattern, const float base, const int i) {
    if (pattern.Count <= 1) {
        return base;
    }
    if (pattern.SpreadDegrees >= 360.0F) {
        return base + pattern.SpreadDegrees / static_cast<float>(pattern.Count) * static_cast<float>(i);
    }
    return base - pattern.SpreadDegrees / 2.0F +
           pattern.SpreadDegrees / static_cast<float>(pattern.Count - 1) * static_cast<float>(i);
}

// One hit from `source` on `target`, by the engine's rules (Hit in Combat.cpp): Defense,
// protection, hit count, one second of protection for a player, and a death noted in
// world.Deaths (a shot counts for its shooter). The source is not used up.
constexpr float kHitInvincibility = 1.0F; // as the engine's
void hurt(World& world, const int source, const int target) {
    const auto health = world.Healths.find(target);
    if (health == world.Healths.end() || health->second.Hp <= 0 || world.Invincibles.contains(target)) {
        return;
    }
    const auto damage = world.Damages.find(source);
    int amount = damage != world.Damages.end() ? damage->second.Dmg : 0;
    const auto stats = world.Stats.find(target);
    if (amount > 0 && stats != world.Stats.end()) {
        amount = std::max(amount - stats->second.Defense, 1);
    }
    health->second.Hp -= amount;
    if (amount > 0) {
        ++health->second.Hits;
    }
    if (health->second.Hp > 0) {
        if (world.Players.contains(target)) {
            world.Invincibles[target].Time = kHitInvincibility;
        }
        return;
    }
    EDeath death;
    death.Id = target;
    const auto owner = world.OwnerIds.find(source);
    death.Killer = world.Bullets.contains(source) && owner != world.OwnerIds.end() ? owner->second.OId : source;
    const auto enemy = world.Enemies.find(target);
    death.EnemyType = enemy != world.Enemies.end() ? enemy->second.TypeId : -1;
    death.Player = world.Players.contains(target);
    death.Position = world.Positions.contains(target) ? world.Positions.at(target).Xy : sf::Vector2f(0, 0);
    world.Deaths.push_back(death);
    world.Destroy(target);
}

// Do the hurtboxes of two entities overlap? (the engine's rule for pickups, in Items.cpp)
bool overlapping(const World& world, const int a, const int b) {
    const auto pa = world.Positions.find(a);
    const auto pb = world.Positions.find(b);
    const auto ha = world.Hurtboxes.find(a);
    const auto hb = world.Hurtboxes.find(b);
    if (pa == world.Positions.end() || pb == world.Positions.end() || ha == world.Hurtboxes.end() ||
        hb == world.Hurtboxes.end()) {
        return false;
    }
    const sf::Vector2f delta = (pa->second.Xy + ha->second.Offset) - (pb->second.Xy + hb->second.Offset);
    const float reach = ha->second.Radius + hb->second.Radius;
    return delta.x * delta.x + delta.y * delta.y <= reach * reach;
}

// The player nearest to `from`, if any.
std::optional<sf::Vector2f> nearest_player(const World& world, const sf::Vector2f from) {
    std::optional<sf::Vector2f> nearest;
    float best = 0.0F;
    for (const auto& [id, player] : world.Players) {
        const auto position = world.Positions.find(id);
        if (position == world.Positions.end()) {
            continue;
        }
        const float distance = std::hypot(position->second.Xy.x - from.x, position->second.Xy.y - from.y);
        if (!nearest || distance < best) {
            nearest = position->second.Xy;
            best = distance;
        }
    }
    return nearest;
}

} // namespace

Game::Game(const GameData& data) : data_(data), world_(data.definitions) {
    if (!data_.levels.empty()) {
        spawned_.assign(data_.levels[level_].waves.size(), 0);
    }
}

void Game::add_ship(const std::uint8_t slot) {
    if (ships_.contains(slot)) {
        return;
    }
    const sf::Vector2f top_left = {100.0F, 150.0F + 120.0F * static_cast<float>(slot - 1)};
    Ship ship;
    ship.entity = CreateEPlayer(world_, top_left + sf::Vector2f(kHalf, kHalf));
    world_.Speeds[ship.entity].Value = config::kPlayerSpeed;
    world_.Spawners[ship.entity].Interval = kShipFireInterval;
    if (data_.invincible) {
        // Temporary test mode also covers respawns; golden shield visuals stay independent.
        world_.Invincibles[ship.entity].Time = std::numeric_limits<float>::infinity();
    }
    ships_.emplace(slot, ship);
}

void Game::remove_ship(const std::uint8_t slot) {
    const auto it = ships_.find(slot);
    if (it == ships_.end()) {
        return;
    }
    world_.Destroy(it->second.entity);
    ships_.erase(it);
}

bool Game::has_ship(const std::uint8_t slot) const {
    return ships_.contains(slot);
}

void Game::set_input(const std::uint8_t slot, const std::uint8_t keys) {
    const auto it = ships_.find(slot);
    if (it == ships_.end()) {
        return;
    }
    EPlayer& player = world_.Players[it->second.entity];
    player.Move = {0.0F, 0.0F};
    if (pressed(keys, InputType::Up)) player.Move.y -= 1.0F;
    if (pressed(keys, InputType::Down)) player.Move.y += 1.0F;
    if (pressed(keys, InputType::Left)) player.Move.x -= 1.0F;
    if (pressed(keys, InputType::Right)) player.Move.x += 1.0F;
    player.Shoot = pressed(keys, InputType::Shoot);
}

void Game::step(const float dt) {
    spawn_waves();
    // Same order as the engine's own frame (Action in System.cpp), without drawing, with the
    // game's own rules around it: burst fire, target zones and power-ups.
    MoveInput(world_);
    const std::vector<BurstHold> bursts = prepare_bursts(dt);
    const Volleys volleys = prepare_volleys(dt);
    Spawner(world_, dt); // before Flight, so new bullets start on their flight pattern
    finish_bursts(bursts);
    finish_volleys(volleys);
    fire_beams(dt);
    Flight(world_, dt);
    keep_casters_reachable(dt);
    Follow(world_, dt);
    Movement(world_, dt);
    for (const auto& [slot, ship] : ships_) {
        sf::Vector2f& position = world_.Positions[ship.entity].Xy;
        // Players move across a fixed viewport while scenery scrolls independently.
        position.x = std::clamp(position.x, kMinCentre, kMaxCentreX);
        position.y = std::clamp(position.y, kMinCentre, kMaxCentreY);
    }
    Collision(world_);
    zone_hits();
    beam_hits();
    const std::vector<std::pair<int, int>> picked = pickups_about_to_happen();
    Pickup(world_);
    apply_pickups(picked);
    Timers(world_, dt);
    count_down(dt);
    Cull(world_);
    clear_zones();
    read_deaths();
    level_time_ += dt;
}

void Game::spawn_waves() {
    if (level_ >= data_.levels.size()) {
        return;
    }
    const Level& level = data_.levels[level_];
    for (std::size_t i = 0; i < level.waves.size(); ++i) {
        const Wave& wave = level.waves[i];
        // Every member of the wave whose entry time has come, in order.
        while (spawned_[i] < wave.count &&
               level_time_ >= wave.time + wave.interval * static_cast<float>(spawned_[i])) {
            const float y = wave.y + wave.y_step * static_cast<float>(spawned_[i]);
            CreateEEnemy(world_, {wave.x, y}, wave.enemy);
            ++spawned_[i];
        }
    }
    if (!boss_spawned_ && level_time_ >= level.boss.time) {
        boss_ = CreateEEnemy(world_, {level.boss.x, level.boss.y}, level.boss.enemy);
        boss_spawned_ = true;
        next_support_time_ = level_time_ + level.support_interval;
    }
    if (boss_ >= 0 && world_.Enemies.contains(boss_) && !level.support_enemies.empty() &&
        level_time_ >= next_support_time_) {
        next_support_time_ = level_time_ + level.support_interval;
        int ordinary = 0;
        for (const auto& [id, enemy] : world_.Enemies) {
            if (id != boss_ && !world_.EnemyTypes.at(enemy.TypeId).Boss) ++ordinary;
        }
        if (ordinary < level.support_max_alive) {
            const int type = level.support_enemies[support_spawned_ % level.support_enemies.size()];
            const float y = support_spawned_ % 2 == 0 ? 150.0f : 570.0f;
            CreateEEnemy(world_, {1320.0f, y}, type);
            ++support_spawned_;
        }
    }
}

// Burst fire: three volleys 0.1 s apart, then a 0.6 s pause; releasing fire cancels the
// pending burst. The game decides when a volley is due and lets the engine's spawner fire it
// (shooting, with its timer full), or not (not shooting). finish_bursts then puts back the
// timer the spawner keeps while a burst replaces it, so the normal cadence resumes after.
std::vector<Game::BurstHold> Game::prepare_bursts(const float dt) {
    std::vector<BurstHold> holds;
    for (auto& [slot, ship] : ships_) {
        const auto spawner = world_.Spawners.find(ship.entity);
        if (ship.burst_time <= 0 || spawner == world_.Spawners.end() || spawner->second.Interval <= 0) {
            continue;
        }
        bool& shoot = world_.Players[ship.entity].Shoot;
        holds.push_back({ship.entity, shoot, std::min(spawner->second.Timer + dt, spawner->second.Interval)});
        ship.burst_timer += dt;
        bool volley = false;
        if (!shoot) {
            ship.burst_remaining = 0;
            ship.burst_timer = std::min(ship.burst_timer, kBurstPause);
        } else if (ship.burst_timer >= (ship.burst_remaining > 0 ? kBurstGap : kBurstPause)) {
            ship.burst_timer = 0;
            if (ship.burst_remaining == 0) {
                ship.burst_remaining = kBurstVolleys;
            }
            --ship.burst_remaining;
            volley = true;
        }
        shoot = volley;
        if (volley) {
            spawner->second.Timer = spawner->second.Interval;
        }
    }
    return holds;
}

void Game::finish_bursts(const std::vector<BurstHold>& holds) {
    for (const BurstHold& hold : holds) {
        world_.Players[hold.entity].Shoot = hold.shoot;
        world_.Spawners[hold.entity].Timer = hold.timer;
    }
}

// Looks at what the engine's spawner will fire this frame, in its own order. Target zones:
// the spawner fires the zone's shot, which finish_volleys turns into a zone; if the limits
// or the lack of a player forbid it, the shot is muted (no pattern: the spawner only
// counts). Patterns with several "origins": muted, and fired by the game from each origin.
Game::Volleys Game::prepare_volleys(const float dt) {
    Volleys volleys;
    volleys.first_new = world_.NextId;
    std::size_t zones = zones_.size();
    for (auto& [id, spawner] : world_.Spawners) {
        const int pattern = spawner.PatternId;
        const bool zone = data_.zones.contains(pattern);
        if ((!zone && !data_.origins.contains(pattern)) || !fires_now(world_, id, spawner, dt)) {
            continue;
        }
        if (zone) {
            int owned = 0;
            for (const auto& [shot, existing] : zones_) {
                owned += existing.owner == id ? 1 : 0;
            }
            const std::optional<sf::Vector2f> target = nearest_player(world_, world_.Positions.at(id).Xy);
            if (zones < kMaxZones && owned < kMaxZonesPerCaster && target) {
                volleys.zones.emplace_back(id, *target);
                ++zones;
                continue;
            }
        }
        volleys.muted.emplace_back(id, pattern);
        spawner.PatternId = -1;
    }
    return volleys;
}

void Game::finish_volleys(const Volleys& volleys) {
    for (const auto& [shooter, pattern] : volleys.muted) {
        world_.Spawners[shooter].PatternId = pattern;
    }
    for (const auto& [caster, where] : volleys.zones) {
        for (const auto& [shot, owner] : world_.OwnerIds) {
            if (shot >= volleys.first_new && owner.OId == caster && world_.Bullets.contains(shot)) {
                make_zone(shot, caster, where);
                break;
            }
        }
    }
    for (const auto& [shooter, pattern] : volleys.muted) {
        if (data_.origins.contains(pattern)) {
            fire_from_origins(shooter, pattern);
        }
    }
}

// One volley from each origin of the pattern (relative to the shooter), each aimed from its
// own origin, by the engine's volley rules (Fire in Behaviour.cpp).
void Game::fire_from_origins(const int shooter, const int pattern_id) {
    const auto pattern = world_.AttackPatterns.find(pattern_id);
    const auto position = world_.Positions.find(shooter);
    if (pattern == world_.AttackPatterns.end() || position == world_.Positions.end()) {
        return;
    }
    const sf::Vector2f centre = position->second.Xy;
    for (const sf::Vector2f& offset : data_.origins.at(pattern_id)) {
        const sf::Vector2f from = centre + offset;
        float base = pattern->second.AngleDegrees;
        if (pattern->second.Aimed) {
            if (const std::optional<sf::Vector2f> target = nearest_player(world_, from)) {
                base = AngleTo(from, *target);
            }
        }
        for (int i = 0; i < pattern->second.Count; ++i) {
            const int bullet = CreateEBullet(world_, shooter);
            if (bullet < 0) {
                return;
            }
            const float angle = bullet_angle(pattern->second, base, i);
            world_.Positions[bullet].Xy = from;
            world_.Velocities[bullet].Xy = Direction(angle) * pattern->second.Speed;
            if (const auto flight = world_.Flights.find(bullet); flight != world_.Flights.end()) {
                flight->second.Origin = from;
                flight->second.Heading = angle;
            }
        }
    }
}

// Enemies with a beam cast it every interval, on their own timer, besides their attack.
void Game::fire_beams(const float dt) {
    std::erase_if(beam_timers_, [this](const auto& timer) { return !world_.Enemies.contains(timer.first); });
    std::vector<std::pair<int, int>> casting; // caster, pattern
    for (const auto& [id, enemy] : world_.Enemies) {
        const auto caster = data_.beam_casters.find(enemy.TypeId);
        if (caster == data_.beam_casters.end()) {
            continue;
        }
        float& timer = beam_timers_[id];
        timer += dt;
        const float interval = data_.beams.at(caster->second).interval;
        if (timer >= interval) {
            timer -= interval;
            casting.emplace_back(id, caster->second);
        }
    }
    for (const auto& [caster, pattern] : casting) {
        const BeamPattern& shape = data_.beams.at(pattern);
        const int id = CreateE(world_);
        world_.Positions[id].Xy = shape.origin + shape.direction * (shape.length / 2.0F); // its centre
        const auto attack = world_.AttackPatterns.find(pattern);
        world_.Damages[id].Dmg = attack != world_.AttackPatterns.end() ? attack->second.Damage : 0;
        world_.Lifetimes[id].Lifetime = shape.warning + shape.duration;
        Beam beam;
        beam.owner = caster;
        beam.pattern = pattern;
        beam.warning = shape.warning;
        beams_[id] = beam;
    }
}

// A beam hurts each player whose hurtbox touches its strip once, after its warning and while
// its caster lives.
void Game::beam_hits() {
    std::vector<std::pair<int, int>> hits; // beam, player
    for (auto& [id, beam] : beams_) {
        const auto owner = world_.Healths.find(beam.owner);
        if (beam.warning > 0 || owner == world_.Healths.end() || owner->second.Hp <= 0) {
            continue;
        }
        const BeamPattern& shape = data_.beams.at(beam.pattern);
        for (const auto& [player, tag] : world_.Players) {
            const auto at = world_.Positions.find(player);
            const auto hurtbox = world_.Hurtboxes.find(player);
            if (beam.hit.contains(player) || at == world_.Positions.end() || hurtbox == world_.Hurtboxes.end()) {
                continue;
            }
            const sf::Vector2f from_origin = at->second.Xy + hurtbox->second.Offset - shape.origin;
            const float along = from_origin.x * shape.direction.x + from_origin.y * shape.direction.y;
            const float across = std::fabs(from_origin.x * shape.direction.y - from_origin.y * shape.direction.x);
            const float r = hurtbox->second.Radius;
            if (along >= -r && along <= shape.length + r && across <= shape.width / 2.0F + r) {
                beam.hit.insert(player);
                hits.emplace_back(id, player);
            }
        }
    }
    for (const auto& [beam, player] : hits) {
        if (world_.Positions.contains(player)) {
            hurt(world_, beam, player);
        }
    }
}

// The caster's shot becomes a zone on the target: it stays put, harmless for its warning
// time, then dangerous for the pattern's lifetime.
void Game::make_zone(const int shot, const int caster, const sf::Vector2f where) {
    const int pattern = world_.Spawners.at(caster).PatternId;
    Zone zone;
    zone.owner = caster;
    zone.warning = data_.zones.at(pattern).warning;
    zone.initial_warning = zone.warning;
    zone.radius = world_.Hitboxes.at(shot).Radius;
    world_.Positions[shot].Xy = where;
    world_.Velocities[shot].Xy = {0.0F, 0.0F};
    world_.Flights.erase(shot);
    world_.Lifetimes[shot].Lifetime = zone.warning + world_.AttackPatterns.at(pattern).Lifetime;
    world_.Hitboxes.erase(shot); // zone_hits, not the engine's Collision, decides when it hurts
    zones_[shot] = zone;
}

// Casters enter the arena, then stay reachable so the players can kill them, which cancels
// their zones before these expire.
void Game::keep_casters_reachable(const float dt) {
    if (dt <= 0) {
        return;
    }
    for (const auto& [id, spawner] : world_.Spawners) {
        if (!world_.Enemies.contains(id) || !data_.zones.contains(spawner.PatternId)) {
            continue;
        }
        const auto flight = world_.Flights.find(id);
        const auto velocity = world_.Velocities.find(id);
        const auto position = world_.Positions.find(id);
        if (flight == world_.Flights.end() || velocity == world_.Velocities.end() || position == world_.Positions.end()) {
            continue;
        }
        const auto pattern = world_.FlightPatterns.find(flight->second.PatternId);
        if (pattern == world_.FlightPatterns.end() || pattern->second.Type == FlightType::Path) {
            continue;
        }
        float& vx = velocity->second.Xy.x;
        if (vx < 0) {
            vx = std::max(vx, (kCasterStopX - position->second.Xy.x) / dt);
        }
    }
}

// A zone hurts once its warning is over and while its caster lives, each target at most every
// two seconds. Hitting does not use it up.
void Game::zone_hits() {
    std::vector<std::pair<int, int>> hits; // zone, target
    for (auto& [id, zone] : zones_) {
        const auto position = world_.Positions.find(id);
        const auto layer = world_.Layers.find(id);
        const auto owner = world_.Healths.find(zone.owner);
        if (zone.warning > 0 || position == world_.Positions.end() || layer == world_.Layers.end() ||
            owner == world_.Healths.end() || owner->second.Hp <= 0) {
            continue;
        }
        for (const auto& [target, hurtbox] : world_.Hurtboxes) {
            const auto target_layer = world_.Layers.find(target);
            const auto at = world_.Positions.find(target);
            if (target_layer == world_.Layers.end() || at == world_.Positions.end() ||
                (layer->second.Mask & target_layer->second.Layer) == 0) {
                continue;
            }
            const sf::Vector2f delta = position->second.Xy - (at->second.Xy + hurtbox.Offset);
            const float reach = zone.radius + hurtbox.Radius;
            if (delta.x * delta.x + delta.y * delta.y > reach * reach) {
                continue;
            }
            float& cooldown = zone.cooldowns[target];
            if (cooldown > 0) {
                continue;
            }
            cooldown = kZoneHitCooldown;
            hits.emplace_back(id, target);
        }
    }
    for (const auto& [zone, target] : hits) {
        if (world_.Positions.contains(target)) {
            hurt(world_, zone, target);
        }
    }
}

// Who the engine's Pickup is about to give which item to, by its own rule (Items.cpp): each
// item goes to the first player in the world's order whose hurtbox touches it.
std::vector<std::pair<int, int>> Game::pickups_about_to_happen() {
    std::vector<std::pair<int, int>> picked; // player, item type
    for (const auto& [item, pickup] : world_.Pickups) {
        for (const auto& [player, tag] : world_.Players) {
            if (overlapping(world_, item, player)) {
                picked.emplace_back(player, pickup.ItemId);
                break;
            }
        }
    }
    return picked;
}

// The game's own item effects (burst fire, golden look), on top of the engine's.
void Game::apply_pickups(const std::vector<std::pair<int, int>>& picked) {
    for (const auto& [player, item] : picked) {
        const auto rules = data_.items.find(item);
        const auto slot = slot_of(player);
        if (rules == data_.items.end() || !slot) {
            continue;
        }
        Ship& ship = ships_.at(*slot);
        ship.burst_time = std::max(ship.burst_time, rules->second.burst_duration);
        ship.golden_time = std::max(ship.golden_time, rules->second.golden_duration);
    }
}

void Game::count_down(const float dt) {
    for (auto& [id, beam] : beams_) {
        beam.warning = std::max(0.0F, beam.warning - dt);
    }
    for (auto& [id, zone] : zones_) {
        zone.warning = std::max(0.0F, zone.warning - dt);
        for (auto& [target, cooldown] : zone.cooldowns) {
            cooldown = std::max(0.0F, cooldown - dt);
        }
    }
    for (auto& [slot, ship] : ships_) {
        ship.burst_time = std::max(0.0F, ship.burst_time - dt);
        ship.golden_time = std::max(0.0F, ship.golden_time - dt);
        if (ship.burst_time == 0) {
            ship.burst_remaining = 0;
            ship.burst_timer = kBurstPause;
        }
    }
}

// Zones and beams go with their caster, and are forgotten once they have expired.
void Game::clear_zones() {
    std::vector<int> orphan_beams;
    for (auto it = beams_.begin(); it != beams_.end();) {
        if (!world_.Positions.contains(it->first)) {
            it = beams_.erase(it);
            continue;
        }
        if (!world_.Positions.contains(it->second.owner)) {
            orphan_beams.push_back(it->first);
        }
        ++it;
    }
    for (const int id : orphan_beams) {
        world_.Destroy(id);
        beams_.erase(id);
    }
    std::vector<int> orphans;
    for (auto it = zones_.begin(); it != zones_.end();) {
        if (!world_.Positions.contains(it->first)) {
            it = zones_.erase(it);
            continue;
        }
        if (!world_.Positions.contains(it->second.owner)) {
            orphans.push_back(it->first);
        }
        ++it;
    }
    for (const int id : orphans) {
        world_.Destroy(id);
        zones_.erase(id);
    }
}

float Game::burst_time(const std::uint8_t slot) const {
    const auto ship = ships_.find(slot);
    return ship == ships_.end() ? 0.0F : ship->second.burst_time;
}

float Game::golden_time(const std::uint8_t slot) const {
    const auto ship = ships_.find(slot);
    return ship == ships_.end() ? 0.0F : ship->second.golden_time;
}

void Game::read_deaths() {
    for (const EDeath& death : world_.Deaths) {
        if (death.Player) {
            if (const auto slot = slot_of(death.Id)) {
                ships_.erase(*slot); // Collision already destroyed the entity.
                protocol::Entity body{*slot, static_cast<std::uint8_t>(protocol::EntityKind::Player),
                                      rounded(death.Position.x - kHalf), rounded(death.Position.y - kHalf)};
                events_.push_back({GameEvent::Kind::Died, *slot, 0, body});
                events_.push_back({GameEvent::Kind::PlayerDied, *slot, 0, {}});
            }
            continue;
        }
        if (death.EnemyType >= 0) {
            protocol::Entity body{network_id(death.Id), static_cast<std::uint8_t>(protocol::EntityKind::Enemy),
                                  rounded(death.Position.x), rounded(death.Position.y)};
            body.type = small(death.EnemyType);
            events_.push_back({GameEvent::Kind::Died, 0, 0, body});
        }
        if (death.EnemyType >= 0) {
            const auto type = world_.EnemyTypes.find(death.EnemyType);
            const auto slot = slot_of(death.Killer);
            // Boss drops would be cleared immediately by the level transition.
            if (slot && type != world_.EnemyTypes.end() && !type->second.Boss && death.Id != boss_) {
                drop_item(death.Position);
            }
            if (slot && type != world_.EnemyTypes.end() && type->second.Score > 0) {
                events_.push_back({GameEvent::Kind::Scored, *slot, static_cast<std::uint32_t>(type->second.Score), {}});
            }
        }
        if (death.Id == boss_) {
            boss_ = -1;
            events_.push_back({GameEvent::Kind::LevelCleared, 0, 0, {}});
        }
    }
    world_.Deaths.clear();
}

void Game::drop_item(sf::Vector2f position) {
    // One weighted roll per kill, with the remaining probability dropping nothing.
    float roll = std::uniform_real_distribution<float>(0.0f, 1.0f)(drop_random_);
    std::vector<int> types;
    for (const auto& [id, item] : world_.ItemTypes) types.push_back(id);
    std::sort(types.begin(), types.end());
    position.x = std::clamp(position.x, kMinCentre, kMaxCentreX);
    position.y = std::clamp(position.y, kMinCentre, kMaxCentreY);
    for (int type : types) {
        const auto rules = data_.items.find(type);
        roll -= rules == data_.items.end() ? 0.0F : rules->second.drop_chance;
        if (roll < 0) {
            CreateEItem(world_, position, type);
            break;
        }
    }
}

std::optional<std::uint8_t> Game::slot_of(const int entity) const {
    for (const auto& [slot, ship] : ships_) {
        if (ship.entity == entity) {
            return slot;
        }
    }
    return std::nullopt;
}

bool Game::next_level() {
    if (level_ + 1 >= data_.levels.size()) {
        return false;
    }
    ++level_;
    level_time_ = 0;
    spawned_.assign(data_.levels[level_].waves.size(), 0);
    boss_spawned_ = false;
    boss_ = -1;
    next_support_time_ = 0;
    support_spawned_ = 0;
    // A new level starts clean: what is left of the previous one goes.
    std::vector<int> leftovers;
    for (const auto& [id, enemy] : world_.Enemies) leftovers.push_back(id);
    for (const auto& [id, bullet] : world_.Bullets) leftovers.push_back(id);
    for (const auto& [id, pickup] : world_.Pickups) leftovers.push_back(id);
    for (const auto& [id, beam] : beams_) leftovers.push_back(id);
    for (const int id : leftovers) world_.Destroy(id);
    zones_.clear();
    beams_.clear();
    beam_timers_.clear();
    return true;
}

std::vector<GameEvent> Game::take_events() {
    std::vector<GameEvent> events;
    events.swap(events_);
    return events;
}

std::uint8_t Game::health_percent(const int entity) const {
    const auto health = world_.Healths.find(entity);
    if (health == world_.Healths.end() || health->second.MaxHp <= 0) {
        return 0;
    }
    return static_cast<std::uint8_t>(std::clamp(health->second.Hp * 100 / health->second.MaxHp, 0, 100));
}

std::uint8_t Game::hits(const int entity) const {
    const auto health = world_.Healths.find(entity);
    return health == world_.Healths.end() ? 0 : static_cast<std::uint8_t>(health->second.Hits & 0xFF);
}

std::optional<protocol::Entity> Game::describe(const int entity) const {
    using protocol::EntityKind;
    protocol::Entity out;
    if (const auto enemy = world_.Enemies.find(entity); enemy != world_.Enemies.end()) {
        out.kind = static_cast<std::uint8_t>(EntityKind::Enemy);
        out.type = small(enemy->second.TypeId);
        out.hp = health_percent(entity);
        out.hits = hits(entity);
    } else if (const auto bullet = world_.Bullets.find(entity); bullet != world_.Bullets.end()) {
        const auto layer = world_.Layers.find(entity);
        const bool ours = layer != world_.Layers.end() && layer->second.Layer == LayerPlayerBullet;
        out.kind = static_cast<std::uint8_t>(ours ? EntityKind::PlayerShot : EntityKind::EnemyShot);
        out.type = small(bullet->second.PatternId);
        if (const auto zone = zones_.find(entity); zone != zones_.end()) {
            if (!world_.Positions.contains(zone->second.owner)) {
                return std::nullopt; // gone with its caster
            }
            const float progress = 1.0f - zone->second.warning / zone->second.initial_warning;
            out.hp = zone->second.warning > 0 ? static_cast<std::uint8_t>(std::clamp(progress * 100.0f, 0.0f, 99.0f)) : 100;
        }
    } else if (const auto beam = beams_.find(entity); beam != beams_.end()) {
        if (!world_.Positions.contains(beam->second.owner)) {
            return std::nullopt; // gone with its caster
        }
        // An enemy shot whose type is the beam pattern; hp tells the warning, as for zones.
        out.kind = static_cast<std::uint8_t>(EntityKind::EnemyShot);
        out.type = small(beam->second.pattern);
        const float initial = data_.beams.at(beam->second.pattern).warning;
        const float progress = initial > 0 ? 1.0f - beam->second.warning / initial : 1.0f;
        out.hp = beam->second.warning > 0 ? static_cast<std::uint8_t>(std::clamp(progress * 100.0f, 0.0f, 99.0f)) : 100;
    } else if (const auto pickup = world_.Pickups.find(entity); pickup != world_.Pickups.end()) {
        out.kind = static_cast<std::uint8_t>(EntityKind::Item);
        out.type = small(pickup->second.ItemId);
    } else {
        return std::nullopt;
    }
    const auto position = world_.Positions.find(entity);
    if (position == world_.Positions.end()) {
        return std::nullopt;
    }
    out.id = network_id(entity);
    out.x = rounded(position->second.Xy.x);
    out.y = rounded(position->second.Xy.y);
    return out;
}

std::vector<protocol::Entity> Game::snapshot() const {
    std::vector<protocol::Entity> entities;
    for (const auto& [slot, ship] : ships_) {
        const sf::Vector2f& centre = world_.Positions.at(ship.entity).Xy;
        protocol::Entity body{slot, config::kPlayerKind, static_cast<std::int32_t>(centre.x - kHalf),
                              static_cast<std::int32_t>(centre.y - kHalf)};
        body.hp = health_percent(ship.entity);
        body.hits = hits(ship.entity);
        if (ship.golden_time > 0) {
            body.type = protocol::kGoldenPlayer;
        }
        entities.push_back(body);
    }
    // The world's maps are unordered: sort the others by engine id, so a snapshot is
    // stable from one frame to the next.
    std::vector<int> others;
    for (const auto& [id, enemy] : world_.Enemies) others.push_back(id);
    for (const auto& [id, bullet] : world_.Bullets) others.push_back(id);
    for (const auto& [id, pickup] : world_.Pickups) others.push_back(id);
    for (const auto& [id, beam] : beams_) others.push_back(id);
    std::sort(others.begin(), others.end());
    for (const int id : others) {
        if (const auto body = describe(id)) {
            entities.push_back(*body);
        }
    }
    return entities;
}

} // namespace ntype::server
