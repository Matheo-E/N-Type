#include "ClientApp.hpp"

#include <exception>

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>

#include "GameConfig.hpp"
#include "Protocol.hpp"
#include "screens/ConnectScreen.hpp"
#include "screens/CreateScreen.hpp"
#include "screens/GameScreen.hpp"
#include "screens/LobbyListScreen.hpp"
#include "screens/WaitingRoomScreen.hpp"

namespace ntype::client {

namespace {

constexpr std::uint8_t bit(const protocol::InputType key) {
    return static_cast<std::uint8_t>(key);
}

} // namespace

ClientApp::ClientApp(ConnectionSettings defaults) : theme_(ui::load_theme()), settings_(std::move(defaults)) {
    actions_.connect = [this](const ConnectionSettings& settings) { return connect(settings); };
    actions_.show_create = [this](const bool creating) { creating_ = creating; };
    create_window();
    show(ScreenId::Connect);
}

void ClientApp::create_window() {
    if (fullscreen_) {
        window_.create(sf::VideoMode::getDesktopMode(), "N-Type", sf::Style::Fullscreen);
    } else {
        window_.create(sf::VideoMode(config::kWidth, config::kHeight), "N-Type", sf::Style::Default);
    }
    window_.setFramerateLimit(60);
    update_view();
}

void ClientApp::update_view() {
    const auto size = window_.getSize();
    if (size.x == 0 || size.y == 0) {
        return;
    }
    // Letterbox the 1280x720 view into the window, keeping its proportions.
    const float window_ratio = static_cast<float>(size.x) / static_cast<float>(size.y);
    const float view_ratio = ui::kMenuWidth / ui::kMenuHeight;
    sf::FloatRect viewport(0.F, 0.F, 1.F, 1.F);
    if (window_ratio > view_ratio) {
        viewport.width = view_ratio / window_ratio;
        viewport.left = (1.F - viewport.width) / 2.F;
    } else {
        viewport.height = window_ratio / view_ratio;
        viewport.top = (1.F - viewport.height) / 2.F;
    }
    sf::View view(sf::FloatRect(0.F, 0.F, ui::kMenuWidth, ui::kMenuHeight));
    view.setViewport(viewport);
    window_.setView(view);
}

std::string ClientApp::connect(const ConnectionSettings& settings) {
    settings_ = settings;
    network_.reset(); // Disconnects the previous session, if any.
    try {
        network_ = std::make_unique<ClientNetwork>(settings.host, settings.port, settings.name);
    } catch (const std::exception&) {
        return "Host not found: " + settings.host;
    }
    return {};
}

ScreenId ClientApp::wanted() const {
    if (!network_ || network_->phase() == Phase::Connecting) {
        return ScreenId::Connect;
    }
    if (network_->phase() == Phase::Browsing) {
        return creating_ ? ScreenId::Create : ScreenId::LobbyList;
    }
    const auto& lobby = network_->lobby();
    return lobby->state == protocol::LobbyPhase::Playing ? ScreenId::Game : ScreenId::WaitingRoom;
}

void ClientApp::show(const ScreenId id) {
    current_ = id;
    switch (id) {
    case ScreenId::Connect:
        screen_ = std::make_unique<ConnectScreen>(theme_, actions_, settings_, network_);
        break;
    case ScreenId::LobbyList:
        screen_ = std::make_unique<LobbyListScreen>(theme_, actions_, *network_);
        break;
    case ScreenId::Create:
        screen_ = std::make_unique<CreateScreen>(theme_, actions_, *network_);
        break;
    case ScreenId::WaitingRoom:
        screen_ = std::make_unique<WaitingRoomScreen>(theme_, *network_);
        break;
    case ScreenId::Game:
        screen_ = std::make_unique<GameScreen>(theme_, *network_);
        break;
    }
}

void ClientApp::run() {
    while (window_.isOpen()) {
        poll_events();
        if (!window_.isOpen()) {
            break;
        }
        if (network_) {
            network_->update(read_input());
            if (network_->phase() != Phase::Browsing) {
                creating_ = false;
            }
        }
        if (const auto id = wanted(); id != current_) {
            show(id);
        }
        screen_->update();
        std::string title = "N-Type | " + settings_.name;
        if (network_) {
            title += " | " + network_->status();
        }
        window_.setTitle(title);
        window_.clear(theme_.background);
        screen_->draw(window_);
        window_.display();
    }
    network_.reset();
}

void ClientApp::on_escape() {
    switch (current_) {
    case ScreenId::Connect:
        window_.close();
        break;
    case ScreenId::LobbyList:
        network_.reset(); // Back to the connection page.
        break;
    case ScreenId::Create:
        network_->clear_error();
        creating_ = false;
        break;
    case ScreenId::WaitingRoom:
    case ScreenId::Game:
        network_->leave();
        break;
    }
}

void ClientApp::poll_events() {
    sf::Event event{};
    while (window_.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            window_.close();
            return;
        }
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
            on_escape();
            if (!window_.isOpen()) {
                return;
            }
            // Leaving a page may destroy what the current screen refers to.
            show(wanted());
            continue;
        }
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::F11) {
            fullscreen_ = !fullscreen_;
            create_window();
            continue;
        }
        if (event.type == sf::Event::Resized) {
            update_view();
            continue;
        }
        const auto pixel = event.type == sf::Event::MouseButtonPressed
                               ? sf::Vector2i{event.mouseButton.x, event.mouseButton.y}
                               : sf::Mouse::getPosition(window_);
        screen_->handle(event, window_.mapPixelToCoords(pixel));
        // A screen may have changed the phase (e.g. connected); switch before the next event.
        if (const auto id = wanted(); id != current_) {
            show(id);
        }
    }
}

std::uint8_t ClientApp::read_input() const {
    if (current_ != ScreenId::Game || !window_.hasFocus() || network_->is_spectator()) {
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

} // namespace ntype::client
