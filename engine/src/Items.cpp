#include "../include/System.hpp"

#include <algorithm>
#include <vector>

namespace {

constexpr float MinFireInterval = 0.05f;

// Do the hurtboxes of two entities overlap? (Items are touched with the player's hurtbox.)
bool Overlapping(World& world, int a, int b) {
    auto pa = world.Positions.find(a);
    auto pb = world.Positions.find(b);
    auto ha = world.Hurtboxes.find(a);
    auto hb = world.Hurtboxes.find(b);
    if (pa == world.Positions.end() || pb == world.Positions.end() ||
        ha == world.Hurtboxes.end() || hb == world.Hurtboxes.end()) {
        return false;
    }
    const sf::Vector2f delta = (pa->second.Xy + ha->second.Offset) - (pb->second.Xy + hb->second.Offset);
    const float reach = ha->second.Radius + hb->second.Radius;
    return delta.x * delta.x + delta.y * delta.y <= reach * reach;
}

}  // namespace

void Pickup(World& world) {
    std::vector<int> taken;
    for (auto& [itemid, pickup] : world.Pickups) {
        for (auto& [playerid, tag] : world.Players) {
            if (Overlapping(world, itemid, playerid)) {
                ApplyItem(world, playerid, pickup.ItemId);
                taken.push_back(itemid);
                break;  // one item, one player
            }
        }
    }
    for (int id : taken) {
        world.Destroy(id);
    }
}

void ApplyItem(World& world, int id, int itemid) {
    auto type = world.ItemTypes.find(itemid);
    if (type == world.ItemTypes.end()) {
        return;
    }
    const ItemData& item = type->second;
    ChangeMaxHp(world, id, item.MaxHp);
    Heal(world, id, item.Heal);
    ChangeSpeed(world, id, item.SpeedBonus);
    ChangeFireInterval(world, id, item.FireInterval);
    if (item.ShootingType >= 0) {
        SetShootingType(world, id, item.ShootingType);
    }
    if (item.Invincibility > 0.0f) {
        GiveInvincibility(world, id, item.Invincibility);
    }
    if (item.BurstDuration > 0.0f && world.Spawners.count(id) > 0) {
        auto& boost = world.Powerups[id];
        boost.BurstTime = std::max(boost.BurstTime, item.BurstDuration);
    }
    if (item.GoldenDuration > 0.0f && world.Players.count(id) > 0) {
        auto& boost = world.Powerups[id];
        boost.GoldenTime = std::max(boost.GoldenTime, item.GoldenDuration);
    }
    ChangeAttack(world, id, item.Attack);
    ChangeDefense(world, id, item.Defense);
}

void Heal(World& world, int id, int amount) {
    auto health = world.Healths.find(id);
    if (health != world.Healths.end() && amount != 0) {
        health->second.Hp = std::min(health->second.Hp + amount, health->second.MaxHp);
    }
}

void ChangeMaxHp(World& world, int id, int amount) {
    auto health = world.Healths.find(id);
    if (health == world.Healths.end() || amount == 0) {
        return;
    }
    health->second.MaxHp = std::max(health->second.MaxHp + amount, 1);
    if (amount > 0) {
        health->second.Hp += amount;
    }
    health->second.Hp = std::min(health->second.Hp, health->second.MaxHp);
}

void ChangeSpeed(World& world, int id, float amount) {
    auto speed = world.Speeds.find(id);
    if (speed != world.Speeds.end()) {
        speed->second.Value = std::max(speed->second.Value + amount, 0.0f);
    }
}

void ChangeFireInterval(World& world, int id, float amount) {
    auto spawner = world.Spawners.find(id);
    if (spawner != world.Spawners.end()) {
        spawner->second.Interval = std::max(spawner->second.Interval + amount, MinFireInterval);
    }
}

void SetShootingType(World& world, int id, int patternid) {
    auto spawner = world.Spawners.find(id);
    if (spawner != world.Spawners.end() && world.AttackPatterns.count(patternid) > 0) {
        spawner->second.PatternId = patternid;
    }
}

void GiveInvincibility(World& world, int id, float seconds) {
    if (world.Positions.count(id) == 0 || seconds <= 0.0f) {
        return;
    }
    EInvincible& protection = world.Invincibles[id];
    protection.Time = std::max(protection.Time, seconds);
}

void ChangeAttack(World& world, int id, int amount) {
    if (world.Positions.count(id) > 0 && amount != 0) {
        world.Stats[id].Attack += amount;
    }
}

void ChangeDefense(World& world, int id, int amount) {
    if (world.Positions.count(id) > 0 && amount != 0) {
        world.Stats[id].Defense += amount;
    }
}
