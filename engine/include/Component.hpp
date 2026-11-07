#pragma once

#include "SFML/Graphics.hpp"

#include <string>
#include <utility>
#include <vector>

// Components are data only. Behaviour lives in the systems.
// An empty class is a "tag": an entity either has it or does not.

// ---- Space ----------------------------------------------------------------

class EPosition {
public:
    sf::Vector2f Xy = {0, 0};
};

class EVelocity {
public:
    sf::Vector2f Xy = {0, 0};
};

class ESpeed {
public:
    float Value = 0;  // movement speed, in pixels per second
};

class ERotation {
public:
    float Degrees = 0;
};

class EParent {
public:
    int ParentId = -1;  // the entity this one follows; -1 means none
};

// Follows a flight pattern (how the entity moves over time). Entities without
// a flight pattern simply do not have this component.
class EFlight {
public:
    int PatternId = -1;
    float Time = 0;                  // seconds since the pattern started
    sf::Vector2f Origin = {0, 0};    // where it started; patterns move relative to it
};

// ---- Collision ------------------------------------------------------------
// Both are circles centred on the entity's position plus Offset.
// Hitbox: deals damage. Hurtbox: receives damage.

class EHitbox {
public:
    float Radius = 0;
    sf::Vector2f Offset = {0, 0};
};

class EHurtbox {
public:
    float Radius = 0;
    sf::Vector2f Offset = {0, 0};
};

// Layer bits. An entity's Layer says what it is, its Mask says what it can hit.
inline constexpr int LayerPlayer = 1 << 0;
inline constexpr int LayerEnemy = 1 << 1;
inline constexpr int LayerPlayerBullet = 1 << 2;
inline constexpr int LayerEnemyBullet = 1 << 3;

// Who can hit whom: a hitbox only affects hurtboxes whose Layer is in its Mask.
class ELayer {
public:
    int Layer = 0;
    int Mask = 0;
};

// Blocks movement (walls, rocks, closed doors).
class ESolid {};

// ---- Drawing --------------------------------------------------------------

// What an entity looks like: part of a shared texture (a sprite sheet cell).
// Only entities with an ESprite are drawn. TextureId is the id from
// textures.json; -1 means no image. The image itself lives in World::Textures.
class ESprite {
public:
    int TextureId = -1;
    sf::IntRect Source;  // the cell of the image to show; empty means the whole image
    sf::Vector2f Scale = {1, 1};
};

class EAnimation {
public:
    int Frame = 0;
    int FrameCount = 1;
    float FrameTime = 0.1f;  // seconds per frame
    float Timer = 0;
    sf::Vector2i FrameSize = {0, 0};  // size of one cell in the sheet
};

// Draw order: the entity with the higher Z is painted on top. No EZIndex counts as 0.
// The layers used by the game (see docs/DRAW_LAYERS.md):
inline constexpr int ZBackground = -10;
inline constexpr int ZEnemy = 0;
inline constexpr int ZBullet = 1;
inline constexpr int ZPlayer = 2;
inline constexpr int ZEffect = 3;
inline constexpr int ZUi = 10;

class EZIndex {
public:
    int Z = 0;  // higher is drawn on top
};

// ---- UI -------------------------------------------------------------------

class EText {
public:
    sf::String Text;
    int FontId = 0;
    unsigned CharSize = 24;
    sf::Color Color = sf::Color::White;
};

class EButton {
public:
    sf::Vector2f Size = {0, 0};
    int ActionId = 0;  // which action a system runs on click
    bool Hovered = false;
    bool Pressed = false;
};

// ---- Identity -------------------------------------------------------------

class EName {
public:
    sf::String Name;
};

class EOwnerId {
public:
    int OId = -1;  // the entity that created this one (a bullet's shooter)
};

class EPlayer {};
class EEnemy {};
class EBullet {};

// ---- Combat ---------------------------------------------------------------

class EHealth {
public:
    int Hp = 0;
    int MaxHp = 0;
};

class EDamage {
public:
    int Dmg = 0;
};

class EInvincible {
public:
    float Time = 0;  // seconds left
};

class ELifetime {
public:
    float Lifetime = 0;  // seconds left; the entity is destroyed at 0
};

// Fires a bullet pattern every Interval seconds.
class ESpawner {
public:
    float Interval = 1.0f;
    float Timer = 0;
    int PatternId = -1;
};

// ---- RPG ------------------------------------------------------------------

class EGridPosition {
public:
    sf::Vector2i Cell = {0, 0};
};

class EStats {
public:
    int Level = 1;
    int Exp = 0;
    int Attack = 0;
    int Defense = 0;
    int Dexterity = 0;
};

class EInventory {
public:
    std::vector<std::pair<int, int>> Items;  // {item id, count}
};

class EDialogue {
public:
    int DialogueId = 0;
};

// Runs an event when the player steps on, or interacts with, this entity.
class ETrigger {
public:
    int EventId = 0;
};
