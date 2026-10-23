#pragma once

#include <unordered_map>

#include "Component.hpp"
//#include "Entity.hpp"

struct World {
    int Next_Id = 0;
    std::unordered_map<int, E_Position> Positions;
    std::unordered_map<int, E_Velocity> Velocities;
    std::unordered_map<int, E_Rectangle> Rectangles;
};