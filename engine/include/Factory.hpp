#pragma once

#include "World.hpp"

int CreateE(World& world);
int CreateEPlayer(World& world, sf::Vector2f pos);
// Builds an enemy from its entry in world.EnemyTypes. Returns -1 if enemyid is unknown.
int CreateEEnemy(World& world, sf::Vector2f pos, int enemyid);
// One bullet fired by spawnerid, built from the attack pattern in that entity's spawner
// (world.AttackPatterns). Returns -1 if the spawner has no known attack pattern.
int CreateEBullet(World& world, int spawnerid);
// One enemy that follows parentid. Building a whole train (the loop, and following
// the parent's past positions) is a system's job: functions take at most 4 parameters.
int CreateEEnemyFollower(World& world, sf::Vector2f pos, int enemyid, int parentid);
int CreateEStatic(World& world, sf::Vector2f pos);
// An item lying on the map, built from world.ItemTypes. Returns -1 if itemid is unknown.
int CreateEItem(World& world, sf::Vector2f pos, int itemid);

// Only the look of a type (sprite, animation, draw layer) on an existing entity, for a
// client that draws what a server tells it. False if the type is unknown, or for a shot
// whose pattern has no sprite.
bool AddEnemyLook(World& world, int id, int enemyid);
bool AddShotLook(World& world, int id, int patternid);
bool AddItemLook(World& world, int id, int itemid);
