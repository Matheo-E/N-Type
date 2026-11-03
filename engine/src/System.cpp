#include "../include/System.hpp"

#include <algorithm>
#include <cmath>

void CreateGame(sf::RenderWindow& window, sf::String name) {
    window.create(sf::VideoMode(WindowWidth, WindowHeight), name);
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
    dt = std::min(dt, 0.05f);  // a long stall (window drag) must not teleport things
    window.clear(sf::Color::Black);
    MoveInput(world, playerid);
    Flight(world, dt);
    Follow(world);
    Spawner(world, dt);
    Movement(world, dt);
    Collision(world);
    Timers(world, dt);
    Cull(world);
    Draw(world, window);
    window.display();
}

void MoveInput(World& world, int playerid) {
    auto it = world.Velocities.find(playerid);
    auto speed = world.Speeds.find(playerid);
    if (it == world.Velocities.end() || speed == world.Speeds.end()) {
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
    it->second.Xy = dir * speed->second.Value;
}

void Movement(World& world, float dt) {
    for (auto& [id, vel] : world.Velocities) {
        auto it = world.Positions.find(id);
        if(it != world.Positions.end()) {
            it->second.Xy += (vel.Xy * dt);
        }
    }
}

namespace {

// Sprites are drawn around their position, like circles.
void DrawSprites(World& world, sf::RenderWindow& window) {
    for (auto& [id, look] : world.TextureIds) {
        auto pos = world.Positions.find(id);
        auto texture = world.Textures.find(look.TextId);
        if (pos == world.Positions.end() || texture == world.Textures.end()) {
            continue;
        }
        sf::Sprite sprite(texture->second);
        if (look.Source.width > 0 && look.Source.height > 0) {
            sprite.setTextureRect(look.Source);
        }
        const sf::FloatRect bounds = sprite.getLocalBounds();
        sprite.setOrigin(bounds.width / 2, bounds.height / 2);
        sprite.setScale(look.Scale);
        sprite.setPosition(pos->second.Xy);
        window.draw(sprite);
    }
}

}  // namespace

// Order: sprites, then rectangles, then circles (bullets end up on top).
void Draw(World& world, sf::RenderWindow& window) {
    DrawSprites(world, window);
    for (auto& [id, rect] : world.Rectangles) {
        auto it = world.Positions.find(id);
        if (it != world.Positions.end()) {
            rect.Rect.setPosition(it->second.Xy);
            window.draw(rect.Rect);
        }
    }
    for (auto& [id, circle] : world.Circles) {
        auto it = world.Positions.find(id);
        if (it != world.Positions.end()) {
            circle.Circle.setPosition(it->second.Xy);
            window.draw(circle.Circle);
        }
    }
}

