#pragma once

#include "World.hpp"

#include <string>

inline constexpr int WindowWidth = 800;
inline constexpr int WindowHeight = 600;

void CreateGame(sf::RenderWindow& window, sf::String name);

// One frame: runs every system below in the right order.
void Action(World& world, sf::RenderWindow& window, float dt, int playerid);

// Input and movement (System.cpp)
void MoveInput(World& world, int playerid);
void Movement(World& world, float dt);
void Draw(World& world, sf::RenderWindow& window);

// Behaviour: what entities do on their own (Behaviour.cpp)
void Flight(World& world, float dt);   // EFlight: sets the velocity from the flight pattern
void Follow(World& world);             // EParent: steers toward the parent
void Spawner(World& world, float dt);  // ESpawner: fires bullet patterns on a timer

// Combat and cleanup (Combat.cpp)
void Collision(World& world);          // hitbox vs hurtbox: damage, bullets removed, deaths
void Timers(World& world, float dt);   // ELifetime and EInvincible countdowns
void Cull(World& world);               // removes bullets and enemies far outside the window
