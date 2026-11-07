#include "../include/System.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

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
    dt = std::min(dt, 0.05f);  // a long stall (dragging the window) must not teleport things
    window.clear(sf::Color::Black);
    world.Actions.Update(world.Keys);
    MoveInput(world, playerid);
    Spawner(world, dt, playerid);  // before Flight, so new bullets start on their flight pattern
    Flight(world, dt, playerid);
    Follow(world, dt);  // after Flight: a follower ignores any flight pattern it has
    Movement(world, dt);
    Collision(world);
    Pickup(world);
    Timers(world, dt);
    Cull(world);
    Animate(world, dt);
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
    if (world.Actions.IsDown(Action::MoveUp)) { dir.y -= 1.0f; }
    if (world.Actions.IsDown(Action::MoveDown)) { dir.y += 1.0f; }
    if (world.Actions.IsDown(Action::MoveLeft)) { dir.x -= 1.0f; }
    if (world.Actions.IsDown(Action::MoveRight)) { dir.x += 1.0f; }

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

int ZOf(World& world, int id) {
    auto z = world.ZIndexes.find(id);
    return z != world.ZIndexes.end() ? z->second.Z : 0;
}

void DrawSprite(World& world, sf::RenderWindow& window, int id) {
    const ESprite& look = world.Sprites.at(id);
    auto pos = world.Positions.find(id);
    auto texture = world.Textures.find(look.TextureId);
    if (pos == world.Positions.end() || texture == world.Textures.end()) {
        return;
    }
    sf::Sprite sprite(texture->second.Image);
    if (look.Source.width > 0 && look.Source.height > 0) {
        sf::IntRect cell = look.Source;
        auto animation = world.Animations.find(id);
        if (animation != world.Animations.end() && !animation->second.Frames.empty()) {
            cell.left += animation->second.Frames[animation->second.Step] * cell.width;
        }
        sprite.setTextureRect(cell);
    }
    const sf::FloatRect bounds = sprite.getLocalBounds();
    sprite.setOrigin(bounds.width / 2, bounds.height / 2);  // the position is the centre
    sprite.setScale(look.FlipX ? -look.Scale.x : look.Scale.x, look.Scale.y);
    sprite.setPosition(pos->second.Xy);
    window.draw(sprite);
}

}  // namespace

// Draws every entity that has an ESprite, lowest EZIndex first (ties: lowest id).
void Draw(World& world, sf::RenderWindow& window) {
    std::vector<int> order;
    for (auto& [id, look] : world.Sprites) {
        order.push_back(id);
    }
    std::sort(order.begin(), order.end(), [&world](int a, int b) {
        return std::make_pair(ZOf(world, a), a) < std::make_pair(ZOf(world, b), b);
    });
    for (int id : order) {
        DrawSprite(world, window, id);
    }
}

void Animate(World& world, float dt) {
    for (auto& [id, animation] : world.Animations) {
        if (animation.Frames.empty() || animation.FrameTime <= 0.0f) {
            continue;
        }
        animation.Timer += dt;
        while (animation.Timer >= animation.FrameTime) {
            animation.Timer -= animation.FrameTime;
            animation.Step = (animation.Step + 1) % animation.Frames.size();
        }
    }
}
