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
    sf::Color Color = sf::Color::White;
    float Speed = 0;
    float FireInterval = 1.0f;  // seconds between volleys
    int TextureId = -1;        // -1: drawn as a plain circle instead of a sprite
    sf::IntRect Source;        // part of the texture to draw (a sprite sheet cell)
    float Scale = 1;
    int AttackPatternId = -1;  // -1: this enemy does not shoot
    int FlightPatternId = -1;  // -1: this enemy has no flight pattern
};
