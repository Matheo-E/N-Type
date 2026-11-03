#include "../include/System.hpp"

#include <vector>

namespace {

constexpr float CullMargin = 64.0f;  // how far outside the window things survive
constexpr float HitInvincibility = 1.0f;  // seconds of protection after the player is hit

bool Touching(World& world, int hitid, int hurtid) {
    auto hitpos = world.Positions.find(hitid);
    auto hurtpos = world.Positions.find(hurtid);
    if (hitpos == world.Positions.end() || hurtpos == world.Positions.end()) {
        return false;
    }
    const EHitbox& hitbox = world.Hitboxes.at(hitid);
    const EHurtbox& hurtbox = world.Hurtboxes.at(hurtid);
    const sf::Vector2f delta = (hitpos->second.Xy + hitbox.Offset) - (hurtpos->second.Xy + hurtbox.Offset);
    const float reach = hitbox.Radius + hurtbox.Radius;
    return delta.x * delta.x + delta.y * delta.y <= reach * reach;
}

// Applies one hit. Entities to destroy are only noted: they are removed after
// the loops, because removing them now would break the iteration.
void Hit(World& world, int hitid, int hurtid, std::vector<int>& dead) {
    if (world.Bullets.count(hitid) > 0) {
        dead.push_back(hitid);
    }
    auto health = world.Healths.find(hurtid);
    if (health == world.Healths.end() || world.Invincibles.count(hurtid) > 0) {
        return;
    }
    auto damage = world.Damages.find(hitid);
    health->second.Hp -= damage != world.Damages.end() ? damage->second.Dmg : 0;
    if (health->second.Hp <= 0) {
        dead.push_back(hurtid);
    } else if (world.Players.count(hurtid) > 0) {
        world.Invincibles[hurtid].Time = HitInvincibility;
    }
}

void DestroyAll(World& world, const std::vector<int>& dead) {
    for (int id : dead) {
        world.Destroy(id);
    }
}

}  // namespace

// Every hitbox against every hurtbox it may hit (its Mask against their Layer).
// Simple and slow: with thousands of bullets this needs a spatial index.
void Collision(World& world) {
    std::vector<int> dead;
    for (auto& [hitid, hitbox] : world.Hitboxes) {
        auto hitlayer = world.Layers.find(hitid);
        if (hitlayer == world.Layers.end()) {
            continue;
        }
        for (auto& [hurtid, hurtbox] : world.Hurtboxes) {
            auto hurtlayer = world.Layers.find(hurtid);
            if (hurtlayer == world.Layers.end() || (hitlayer->second.Mask & hurtlayer->second.Layer) == 0) {
                continue;
            }
            if (Touching(world, hitid, hurtid)) {
                Hit(world, hitid, hurtid, dead);
                if (world.Bullets.count(hitid) > 0) {
                    break;  // a bullet hits only once
                }
            }
        }
    }
    DestroyAll(world, dead);
}

void Timers(World& world, float dt) {
    std::vector<int> dead;
    for (auto& [id, life] : world.Lifetimes) {
        life.Lifetime -= dt;
        if (life.Lifetime <= 0) {
            dead.push_back(id);
        }
    }
    for (auto it = world.Invincibles.begin(); it != world.Invincibles.end();) {
        it->second.Time -= dt;
        it = it->second.Time <= 0 ? world.Invincibles.erase(it) : std::next(it);
    }
    DestroyAll(world, dead);
}

void Cull(World& world) {
    std::vector<int> dead;
    for (auto& [id, pos] : world.Positions) {
        if (world.Bullets.count(id) == 0 && world.Enemies.count(id) == 0) {
            continue;
        }
        const sf::Vector2f p = pos.Xy;
        if (p.x < -CullMargin || p.x > WindowWidth + CullMargin ||
            p.y < -CullMargin || p.y > WindowHeight + CullMargin) {
            dead.push_back(id);
        }
    }
    DestroyAll(world, dead);
}
