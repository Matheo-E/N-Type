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

ClientApp::ClientApp(const std::string& name, const std::string& host, const std::uint16_t port)
    : name_(name), network_(host, port) {
    create_window();
}

void ClientApp::create_window() {
    if (fullscreen_) {
        window_.create(sf::VideoMode::getDesktopMode(), "N-Type | Connecting", sf::Style::Fullscreen);
    } else {
        window_.create(sf::VideoMode(config::kWidth, config::kHeight), "N-Type | Connecting",
                       sf::Style::Default);
    }
    window_.setFramerateLimit(60);
    update_view();
}

void ClientApp::update_view() {
    const auto size = window_.getSize();
    if (size.x == 0 || size.y == 0) return;
    const float window_ratio = static_cast<float>(size.x) / static_cast<float>(size.y);
    const float game_ratio = static_cast<float>(config::kWidth) / static_cast<float>(config::kHeight);
    sf::FloatRect viewport(0.f, 0.f, 1.f, 1.f);
    if (window_ratio > game_ratio) {
        viewport.width = game_ratio / window_ratio;
        viewport.left = (1.f - viewport.width) / 2.f;
    } else {
        viewport.height = window_ratio / game_ratio;
        viewport.top = (1.f - viewport.height) / 2.f;
    }
    sf::View view(sf::FloatRect(0.f, 0.f, static_cast<float>(config::kWidth),
                               static_cast<float>(config::kHeight)));
    view.setViewport(viewport);
    window_.setView(view);
}

void ClientApp::run() {
    while (window_.isOpen()) {
        poll_events();
        if (!window_.isOpen()) {
            break;
        }
        network_.update(read_input());
        std::string title = "N-Type | " + name_ + " | " + network_.status();
        if (const auto id = network_.id()) {
            title += std::string(" (") + kColorNames[*id - 1U] + ")";
        }
        window_.setTitle(title + " | Arrows/WASD/ZQSD | F11: fullscreen | Esc: quit");
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
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::F11) {
            fullscreen_ = !fullscreen_;
            create_window();
        }
        if (event.type == sf::Event::Resized) update_view();
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
