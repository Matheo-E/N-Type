#include "../include/System.hpp"

#include <cmath>

void Create_Game(sf::RenderWindow& window, sf::String name) {
    window.create(sf::VideoMode(800, 600), name);
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);
}

void Action(World& world, sf::RenderWindow& window, float dt, int playerid) {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            window.close();
        }
    }
    window.clear(sf::Color::Black);
    Move_Input(world, playerid);
    Movement(world, dt);
    Draw(world, window);
    window.display();
}

void Move_Input(World& world, int playerid) {
    constexpr float Player_Speed = 200.0f;

    auto it = world.Velocities.find(playerid);
    if (it == world.Velocities.end()) {
        return;
    }

    sf::Vector2f dir = {0.0f, 0.0f};
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Z)) { dir.y -= 1.0f; }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) { dir.y += 1.0f; }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q)) { dir.x -= 1.0f; }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) { dir.x += 1.0f; }

    // No key held: dir is still {0, 0}, so the velocity becomes {0, 0}.
    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len > 0.0f) {
        dir /= len;  // diagonals would otherwise be ~1.4x faster
    }
    it->second.Xy = dir * Player_Speed;
}

void Movement(World& world, float dt) {
    for (auto& [id, vel] : world.Velocities) {
        auto it = world.Positions.find(id);
        if(it != world.Positions.end()) {
            it->second.Xy += (vel.Xy * dt);
        }
    }
}

void Draw(World& world, sf::RenderWindow& window) {
    for (auto& [id, rect] : world.Rectangles) {
        auto it = world.Positions.find(id);
        if (it != world.Positions.end()) {
            rect.Rect.setPosition(it->second.Xy);
            window.draw(rect.Rect);
        }
    }
}

