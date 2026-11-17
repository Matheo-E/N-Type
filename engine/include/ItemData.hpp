#pragma once

#include "SFML/Graphics.hpp"

#include <string>
#include <vector>

// What an item type looks like in assets/data/items.json: something on the map that a
// player picks up by touching it. It never deals damage. Like EnemyData, it is a definition
// to build entities from (CreateEItem), not a component.
class ItemData {
public:
    std::string Name = "item";
    float Radius = 16;  // how close the player has to get to pick it up

    // Look, like an enemy's
    int TextureId = -1;
    sf::IntRect Source;
    float Scale = 1;
    bool FlipX = false;
    std::vector<int> AnimationFrames;
    float AnimationFrameTime = 0.1f;

    // Movement and life on the map
    int FlightPatternId = -1;  // -1: stays where it was dropped
    float Speed = 0;           // base speed for its flight pattern
    float Lifetime = 0;        // seconds before it disappears by itself; 0: never

    // What it does to the player who picks it up. 0 (or -1 for ShootingType) means no change.
    int Heal = 0;
    int MaxHp = 0;
    float SpeedBonus = 0;      // added to the player's speed (pixels per second)
    float FireInterval = 0;    // added to the seconds between shots: negative fires faster
    int ShootingType = -1;     // attack pattern the player fires from now on
    float Invincibility = 0;   // seconds of protection
    int Attack = 0;            // added to the damage of the player's bullets
    int Defense = 0;           // removed from the damage the player takes
};
