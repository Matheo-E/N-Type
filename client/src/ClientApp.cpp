#include "ClientApp.hpp"

#include <algorithm>
#include <array>

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>

#include "Factory.hpp"
#include "GameConfig.hpp"
#include "Protocol.hpp"
#include "System.hpp"

namespace ntype::client {

namespace {

// One colour per player, indexed by player id - 1.
const std::array<sf::Color, config::kMaxPlayers> kColors{
    sf::Color(80, 180, 255), sf::Color(255, 160, 60), sf::Color(110, 220, 110), sf::Color(240, 100, 200)};
const std::array<const char*, config::kMaxPlayers> kColorNames{"blue", "orange", "green", "pink"};

constexpr std::uint8_t bit(const protocol::InputType key) {
    return static_cast<std::uint8_t>(key);
}

} // namespace

ClientApp::ClientApp(const std::string& host, const std::uint16_t port)
    : network_(host, port),
      window_(sf::VideoMode(config::kWidth, config::kHeight), "N-Type | Connecting",
              sf::Style::Titlebar | sf::Style::Close) {
    window_.setFramerateLimit(60);
}

void ClientApp::run() {
    while (window_.isOpen()) {
        poll_events();
        if (!window_.isOpen()) {
            break;
        }
        network_.update(read_input());
        std::string title = "N-Type | " + network_.status();
        if (const auto id = network_.id()) {
            title += std::string(" (") + kColorNames[*id - 1U] + ")";
        }
        window_.setTitle(title + " | Arrows/WASD/ZQSD | Esc: quit");
        render();
    }
    network_.disconnect();
}

void ClientApp::poll_events() {
    sf::Event event{};
    while (window_.pollEvent(event)) {
        if (event.type == sf::Event::Closed ||
            (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)) {
            window_.close();
            return;
        }
    }
}

std::uint8_t ClientApp::read_input() const {
    if (!window_.hasFocus()) {
        return 0;
    }
    using Key = sf::Keyboard::Key;
    using protocol::InputType;
    const auto pressed = [](const Key key) { return sf::Keyboard::isKeyPressed(key); };
    std::uint8_t bits = 0;
    if (pressed(Key::Up) || pressed(Key::W) || pressed(Key::Z)) {
        bits |= bit(InputType::Up);
    }
    if (pressed(Key::Down) || pressed(Key::S)) {
        bits |= bit(InputType::Down);
    }
    if (pressed(Key::Left) || pressed(Key::A) || pressed(Key::Q)) {
        bits |= bit(InputType::Left);
    }
    if (pressed(Key::Right) || pressed(Key::D)) {
        bits |= bit(InputType::Right);
    }
    if (pressed(Key::Space)) {
        bits |= bit(InputType::Shoot);
    }
    return bits;
}

void ClientApp::sync_world() {
    const auto& players = network_.players();
    // Only players in the latest snapshot keep an entity.
    for (auto it = entities_.begin(); it != entities_.end();) {
        const auto id = it->first;
        if (std::none_of(players.begin(), players.end(), [id](const auto& player) { return player.id == id; })) {
            world_.Positions.erase(it->second);
            world_.Rectangles.erase(it->second);
            it = entities_.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& player : players) {
        auto [it, added] = entities_.try_emplace(player.id, 0);
        if (added) {
            it->second = Create_E(world_);
            auto& square = world_.Rectangles[it->second].Rect;
            square.setSize({config::kPlayerSize, config::kPlayerSize});
            square.setFillColor(kColors[player.id - 1U]);
        }
        // The server is authoritative: positions are copied, never simulated here.
        world_.Positions[it->second].Xy = {static_cast<float>(player.x), static_cast<float>(player.y)};
    }
}

void ClientApp::render() {
    sync_world();
    window_.clear(sf::Color::Black);
    Draw(world_, window_);
    window_.display();
}

} // namespace ntype::client
