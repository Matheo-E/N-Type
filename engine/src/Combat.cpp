#include "../include/System.hpp"

#include <algorithm>
#include <vector>

namespace {

constexpr float HitInvincibility = 1.0f;  // seconds of protection after the player is hit

// Do the hitbox of one entity and the hurtbox of another overlap? Both are circles
// centred on the entity's position plus their Offset.
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

// Applies one hit: the bullet is used up, and the target loses health unless it is
// protected. Entities to remove are only noted in `dead`: removing them now would break
// the loops that are still walking the maps.
void Hit(World& world, int hitid, int hurtid, std::vector<int>& dead) {
    if (world.Bullets.count(hitid) > 0) {
        dead.push_back(hitid);
    }
    auto health = world.Healths.find(hurtid);
    if (health == world.Healths.end() || world.Invincibles.count(hurtid) > 0) {
        return;
    }
    auto damage = world.Damages.find(hitid);
    int amount = damage != world.Damages.end() ? damage->second.Dmg : 0;
    auto stats = world.Stats.find(hurtid);
    if (amount > 0 && stats != world.Stats.end()) {  // Defense softens a hit, but a hit always hurts
        amount = std::max(amount - stats->second.Defense, 1);
    }
    health->second.Hp -= amount;
    if (health->second.Hp <= 0) {
        dead.push_back(hurtid);
    } else if (world.Players.count(hurtid) > 0) {
        world.Invincibles[hurtid].Time = HitInvincibility;
    }
}

}  // namespace

// Every hitbox against every hurtbox it may hit: the hitbox owner's Mask must contain the
// hurtbox owner's Layer (so enemy bullets only hurt the player, and the player's only hurt
// enemies). Simple and slow: with thousands of bullets this wants a spatial index.
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
    for (int id : dead) {
        world.Destroy(id);
    }
}
