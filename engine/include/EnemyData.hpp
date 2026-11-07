#pragma once

#include "SFML/Graphics.hpp"

#include <string>

// What an enemy type looks like in assets/data/enemies.json.
// This is a definition to build entities from, not a component.
class EnemyData {
public:
    std::string Name = "enemy";
    int Hp = 1;
    float Radius = 16;
    int TextureId = -1;  // id in textures.json; -1: the enemy is not drawn
    sf::IntRect Source;  // the cell of the image to show
    float Scale = 1;
    float Speed = 0;
    float FireInterval = 1.0f;  // seconds between volleys
    int AttackPatternId = -1;  // -1: this enemy does not shoot
    int FlightPatternId = -1;  // -1: this enemy has no flight pattern
};
