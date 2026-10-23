#include "../include/Factory.hpp"

int Create_E(World& world) {
    int id = world.Next_Id++;
    return id;
}

int Create_E_Player(World& world, sf::Vector2f pos) {
    int id = Create_E(world);
    world.Positions[id].Xy = pos;
    world.Velocities[id].Xy = {10.0f, 0.0f};
    world.Rectangles[id].Rect.setSize({32.0f, 32.0f});
    world.Rectangles[id].Rect.setFillColor(sf::Color::Green);
    return id;
}

int Create_E_Static(World& world, sf::Vector2f pos) {
    int id = Create_E(world);
    world.Positions[id].Xy = pos;
    return id;
}
