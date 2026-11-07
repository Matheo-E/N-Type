#pragma once

#include "World.hpp"

#include <string>

// The play area (the same as the game's config::kWidth / kHeight).
inline constexpr int WindowWidth = 1280;
inline constexpr int WindowHeight = 720;

void CreateGame(sf::RenderWindow& window, sf::String name);

// One frame of a local, single-window game: playerid is moved by the keyboard.
void Action(World& world, sf::RenderWindow& window, float dt, int playerid);
void ReadKeyboard(World& world, int playerid);  // the keyboard's actions into that player's EPlayer
void MoveInput(World& world);                   // every player's velocity from its EPlayer::Move
void Movement(World& world, float dt);
void Draw(World& world, sf::RenderWindow& window);
void Animate(World& world, float dt);  // EAnimation: steps through the frames

// What entities do on their own, from the loaded patterns (Behaviour.cpp)
void Flight(World& world, float dt);   // EFlight: sets the velocity from the flight pattern (chasers: nearest player)
void Spawner(World& world, float dt);  // ESpawner: fires on a timer (players: while EPlayer::Shoot); aimed: nearest player
void Follow(World& world, float dt);                 // EParent: heads for the parent, stops close to it
void Timers(World& world, float dt);                 // ELifetime and EInvincible countdowns
void Cull(World& world);                             // removes bullets/enemies far outside the window

// Damage (Combat.cpp)
void Collision(World& world);  // hitbox vs hurtbox: damage, used-up bullets, deaths (noted in world.Deaths)

// Items and the player's stats (Items.cpp). Every function does nothing if the entity lacks
// what it changes, and can be called by the game directly, not only through items.
void Pickup(World& world);                                // a player touching an item gets its effects; the item goes
void ApplyItem(World& world, int id, int itemid);         // all the effects of an item type, on entity id
void Heal(World& world, int id, int amount);              // up to MaxHp
void ChangeMaxHp(World& world, int id, int amount);       // raising it also heals by the same amount
void ChangeSpeed(World& world, int id, float amount);     // ESpeed, never below 0
void ChangeFireInterval(World& world, int id, float amount);  // seconds between shots, never below 0.05
void SetShootingType(World& world, int id, int patternid);   // the attack pattern it fires (if it exists)
void GiveInvincibility(World& world, int id, float seconds);  // keeps whichever protection lasts longer
void ChangeAttack(World& world, int id, int amount);      // added to the damage of its bullets
void ChangeDefense(World& world, int id, int amount);     // removed from the damage it takes
