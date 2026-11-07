#pragma once

#include "World.hpp"

#include <string>

inline constexpr int WindowWidth = 800;
inline constexpr int WindowHeight = 600;

void CreateGame(sf::RenderWindow& window, sf::String name);

void Action(World& world, sf::RenderWindow& window, float dt, int playerid);
void MoveInput(World& world, int playerid);
void Movement(World& world, float dt);
void Draw(World& world, sf::RenderWindow& window);
void Animate(World& world, float dt);  // EAnimation: steps through the frames

// What entities do on their own, from the loaded patterns (Behaviour.cpp)
void Flight(World& world, float dt, int playerid);   // EFlight: sets the velocity from the flight pattern
void Spawner(World& world, float dt, int playerid);  // ESpawner: fires the attack pattern on a timer
void Timers(World& world, float dt);                 // ELifetime and EInvincible countdowns
void Cull(World& world);                             // removes bullets/enemies far outside the window

// Damage (Combat.cpp)
void Collision(World& world);  // hitbox vs hurtbox: damage, used-up bullets, deaths
