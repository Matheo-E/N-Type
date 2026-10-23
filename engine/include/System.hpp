#pragma once

#include "World.hpp"

#include <string>

void Create_Game(sf::RenderWindow& window, sf::String name);

void Action(World& world, sf::RenderWindow& window, float dt, int playerid);
void Move_Input(World& world, int playerid);
void Movement(World& world, float dt);
void Draw(World& world, sf::RenderWindow& window);
