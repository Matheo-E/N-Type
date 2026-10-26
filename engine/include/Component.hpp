#pragma once

#include "SFML/Graphics.hpp"

#include <string>

struct E_Position {
    sf::Vector2f Xy = {0, 0};
};

struct E_Hitbox {
    //what should i put
};

struct E_Hurtbox {
    //same
};

struct E_Velocity {
    sf::Vector2f Xy = {0, 0};
};

struct E_Rectangle {
    sf::RectangleShape Rect;
};

struct E_Circle {
    sf::CircleShape Circle;
};

struct E_Convex {
    sf::ConvexShape Convex;
};

struct E_Texture_Id {
    int Text_Id = 0;
};

struct E_Text {
    sf::String Text;
    sf::Vector2f Xy;
};

struct E_Button {
    //im not sure if im pushing too far, idk what i sould put in there
};

struct E_Name {
    sf::String Name;
};

struct E_Lifetime {
    float Lifetime = 0;
};

struct E_Damage {
    int Dmg = 0;
};

struct E_Health {
    int Hp = 0;
};

struct E_OwnerId {
    int O_Id = 0;
};