#pragma once

#include <cstdint>
#include <memory>

#include <SFML/Graphics/RenderWindow.hpp>

#include "ClientNetwork.hpp"
#include "screens/Screen.hpp"
#include "ui/Theme.hpp"

namespace ntype::client {

// The only window: shows the screen that matches the session's phase, from
// the connection page through the lobbies into the game.
class ClientApp {
public:
    explicit ClientApp(ConnectionSettings defaults);
    void run();

private:
    void create_window();
    void update_view();
    void poll_events();
    void on_escape();
    std::uint8_t read_input() const;
    ScreenId wanted() const;
    void show(ScreenId id);
    std::string connect(const ConnectionSettings& settings);

    ui::Theme theme_;
    ConnectionSettings settings_;
    std::unique_ptr<ClientNetwork> network_;
    AppActions actions_;
    bool creating_ = false;
    sf::RenderWindow window_;
    bool fullscreen_ = false;
    ScreenId current_ = ScreenId::Connect;
    std::unique_ptr<Screen> screen_;
};

} // namespace ntype::client
