#include "ClientApp.hpp"

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>

#include "Protocol.hpp"

namespace ntype::client {

ClientApp::ClientApp(const std::string& host, const std::uint16_t port)
    : network_(host, port),
      window_(sf::VideoMode({protocol::kWidth, protocol::kHeight}), "N-Type | Connecting",
              sf::Style::Titlebar | sf::Style::Close) {
    window_.setFramerateLimit(60);
    for (auto& square : squares_) {
        square.setSize({protocol::kSquareSize, protocol::kSquareSize});
    }
    squares_[0].setFillColor(sf::Color(80, 180, 255));
    squares_[1].setFillColor(sf::Color(255, 160, 60));
}

void ClientApp::run() {
    while (window_.isOpen()) {
        poll_events();
        if (!window_.isOpen()) {
            break;
        }
        network_.update(read_input());
        std::string title = "N-Type | " + network_.status();
        if (network_.state()) {
            title += network_.state()->slot == 0 ? " (blue)" : " (orange)";
        }
        window_.setTitle(title + " | Arrows/WASD/ZQSD | Esc: quit");
        render();
    }
    network_.disconnect();
}

void ClientApp::poll_events() {
    while (const auto event = window_.pollEvent()) {
        const auto* key = event->getIf<sf::Event::KeyPressed>();
        if (event->is<sf::Event::Closed>() ||
            (key != nullptr && key->code == sf::Keyboard::Key::Escape)) {
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
    const auto pressed = [](const Key key) { return sf::Keyboard::isKeyPressed(key); };
    std::uint8_t input = 0;
    if (pressed(Key::Left) || pressed(Key::A) || pressed(Key::Q)) {
        input |= protocol::kLeft;
    }
    if (pressed(Key::Right) || pressed(Key::D)) {
        input |= protocol::kRight;
    }
    if (pressed(Key::Up) || pressed(Key::W) || pressed(Key::Z)) {
        input |= protocol::kUp;
    }
    if (pressed(Key::Down) || pressed(Key::S)) {
        input |= protocol::kDown;
    }
    return input;
}

void ClientApp::render() {
    window_.clear(sf::Color::Black);
    if (const auto& state = network_.state()) {
        for (std::size_t slot = 0; slot < squares_.size(); ++slot) {
            // Only players currently connected to the server are visible.
            if ((state->bits & (1U << slot)) == 0) {
                continue;
            }
            squares_[slot].setPosition({static_cast<float>(state->positions[slot].x),
                                        static_cast<float>(state->positions[slot].y)});
            window_.draw(squares_[slot]);
        }
    }
    window_.display();
}

} // namespace ntype::client
