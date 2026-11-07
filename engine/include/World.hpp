#pragma once

#include <unordered_map>

#include "Component.hpp"
#include "EnemyData.hpp"
#include "AllTextures.hpp"

// One map per component type, keyed by entity id.
class World {
public:
    int NextId = 0;

    // Loaded definitions (not per-entity, so not touched by Destroy).
    std::unordered_map<int, EnemyData> EnemyTypes;
    std::unordered_map<int, AllTexture> Textures;

    // Space
    std::unordered_map<int, EPosition> Positions;
    std::unordered_map<int, EVelocity> Velocities;
    std::unordered_map<int, ESpeed> Speeds;
    std::unordered_map<int, ERotation> Rotations;
    std::unordered_map<int, EParent> Parents;
    std::unordered_map<int, EFlight> Flights;

    // Collision
    std::unordered_map<int, EHitbox> Hitboxes;
    std::unordered_map<int, EHurtbox> Hurtboxes;
    std::unordered_map<int, ELayer> Layers;
    std::unordered_map<int, ESolid> Solids;

    // Drawing
    std::unordered_map<int, ESprite> Sprites;
    std::unordered_map<int, EAnimation> Animations;
    std::unordered_map<int, EZIndex> ZIndexes;

    // UI
    std::unordered_map<int, EText> Texts;
    std::unordered_map<int, EButton> Buttons;

    // Identity
    std::unordered_map<int, EName> Names;
    std::unordered_map<int, EOwnerId> OwnerIds;
    std::unordered_map<int, EPlayer> Players;
    std::unordered_map<int, EEnemy> Enemies;
    std::unordered_map<int, EBullet> Bullets;

    // Combat
    std::unordered_map<int, EHealth> Healths;
    std::unordered_map<int, EDamage> Damages;
    std::unordered_map<int, EInvincible> Invincibles;
    std::unordered_map<int, ELifetime> Lifetimes;
    std::unordered_map<int, ESpawner> Spawners;

    // RPG
    std::unordered_map<int, EGridPosition> GridPositions;
    std::unordered_map<int, EStats> Stats;
    std::unordered_map<int, EInventory> Inventories;
    std::unordered_map<int, EDialogue> Dialogues;
    std::unordered_map<int, ETrigger> Triggers;

    // Removes the entity from every map. Add a line here for each new map.
    void Destroy(int id) {
        Positions.erase(id);
        Velocities.erase(id);
        Speeds.erase(id);
        Rotations.erase(id);
        Parents.erase(id);
        Flights.erase(id);

        Hitboxes.erase(id);
        Hurtboxes.erase(id);
        Layers.erase(id);
        Solids.erase(id);

        Sprites.erase(id);
        Animations.erase(id);
        ZIndexes.erase(id);

        Texts.erase(id);
        Buttons.erase(id);

        Names.erase(id);
        OwnerIds.erase(id);
        Players.erase(id);
        Enemies.erase(id);
        Bullets.erase(id);

        Healths.erase(id);
        Damages.erase(id);
        Invincibles.erase(id);
        Lifetimes.erase(id);
        Spawners.erase(id);

        GridPositions.erase(id);
        Stats.erase(id);
        Inventories.erase(id);
        Dialogues.erase(id);
        Triggers.erase(id);
    }
};
