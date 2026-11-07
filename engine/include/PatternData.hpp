#pragma once

#include "SFML/Graphics.hpp"

#include <string>
#include <vector>

// Definitions loaded from assets/data/attack_patterns.json and flight_patterns.json.
// Like EnemyData, they are blueprints a system reads, not components.
//
// Angles are in degrees: 0 points straight down, 90 to the right, 180 up, -90 left.

// How an entity fires: one "volley" is Count bullets spread over SpreadDegrees,
// centred on AngleDegrees (or on the direction of the player when Aimed).
//   Count 3, SpreadDegrees 40, AngleDegrees 0  -> a fan of 3 pointing down
//   Count 16, SpreadDegrees 360                -> a ring (space the bullets by
//                                                 Spread / Count so the first and last
//                                                 do not overlap)
// The rest describes each bullet.
class AttackPattern {
public:
    std::string Name = "attack";
    int Count = 1;
    float SpreadDegrees = 0;
    float AngleDegrees = 0;
    bool Aimed = false;
    float Speed = 200;     // pixels per second
    int Damage = 1;
    float Radius = 4;      // hitbox radius
    float Lifetime = 10;   // seconds
    int TextureId = -1;    // id in textures.json; -1: the bullet is not drawn
    sf::IntRect Source;    // the cell of the image to show
    float Scale = 1;
    bool FlipX = false;    // draw the bullet mirrored
    std::vector<int> AnimationFrames;  // empty: not animated (see EAnimation)
    float AnimationFrameTime = 0.1f;
    // How each bullet moves, as a flight pattern (flight_patterns.json), written for a bullet
    // fired straight down: the pattern is turned to follow the bullet's own firing angle.
    // -1: the bullet just flies in a straight line along its angle at Speed.
    int FlightPatternId = -1;
};

enum class FlightType {
    Stationary,  // does not move
    Straight,    // moves along DirectionDegrees
    Sine,        // moves along DirectionDegrees while swaying from side to side
    Chase,       // moves toward the player
    Path         // follows a list of steps (see PathStep)
};

// One step of a Path flight. All positions are relative to where the entity spawned,
// and each step starts where the previous one ended (the first starts at the spawn point).
// Every step lasts Duration seconds (> 0). After the last step the entity stays put.
enum class StepKind {
    Move,   // a straight line to To
    Curve,  // a smooth cubic Bezier curve to To, bent by Control1 and Control2
    Arc,    // along a circle of Radius, from the current point. StartDegrees is the angle
            // from the circle's centre to the entity at the start (0 = the entity is below
            // the centre); it then turns SweepDegrees (positive: counterclockwise on screen,
            // 360: a full loop that ends where it started)
    Wait    // stays where it is
};

class PathStep {
public:
    StepKind Kind = StepKind::Wait;
    float Duration = 1;
    sf::Vector2f To;                 // Move, Curve
    sf::Vector2f Control1, Control2; // Curve
    float Radius = 0;                // Arc
    float StartDegrees = 0;
    float SweepDegrees = 0;
};

// How an entity moves over time. The entity's own ESpeed is the base speed.
//   Forward: multiplier of ESpeed for the movement along the direction (0: stay in place)
//   Amplitude, Frequency: for Sine, the sideways swing. Amplitude is a speed in pixels
//   per second, perpendicular to the direction. Frequency is in swings per second.
//   Sideways speed at time t = Amplitude * cos(2 * pi * Frequency * t)
class FlightPattern {
public:
    std::string Name = "flight";
    FlightType Type = FlightType::Stationary;
    float DirectionDegrees = 0;
    float Forward = 1;
    float Amplitude = 0;
    float Frequency = 0;
    std::vector<PathStep> Steps;  // Path only
};
