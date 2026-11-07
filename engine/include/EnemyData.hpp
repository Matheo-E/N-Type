#pragma once

#include "SFML/Graphics.hpp"

#include <string>
#include <vector>

// What an enemy type looks like in assets/data/enemies.json.
// This is a definition to build entities from, not a component.
class EnemyData {
public:
    std::string Name = "enemy";
    int Hp = 1;
    int ContactDamage = 0;  // damage dealt to the player on touch; 0: touching is harmless
    float Radius = 16;
    int TextureId = -1;  // id in textures.json; -1: the enemy is not drawn
    sf::IntRect Source;  // the cell of the image to show
    float Scale = 1;
    bool FlipX = false;
    std::vector<int> AnimationFrames;  // empty: not animated (see EAnimation)
    float AnimationFrameTime = 0.1f;
    float Speed = 0;
    float FireInterval = 1.0f;  // seconds between volleys
    int AttackPatternId = -1;  // -1: this enemy does not shoot
    int FlightPatternId = -1;  // -1: this enemy has no flight pattern
};
