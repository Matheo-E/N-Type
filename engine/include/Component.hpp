#pragma once

#include "SFML/Graphics.hpp"

struct E_Position {
    sf::Vector2f Xy = {0, 0};
};

struct E_Velocity {
    sf::Vector2f Xy = {0, 0};
};

struct E_Rectangle {
    sf::RectangleShape Rect;
};