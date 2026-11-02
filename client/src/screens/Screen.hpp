#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>

namespace ntype::client {

struct ConnectionSettings {
    std::string name;
    std::string host;
    std::uint16_t port = 0;
};

enum class ScreenId { Connect, LobbyList, Create, WaitingRoom, Game };

// What screens may ask of the app, besides the network's own actions.
struct AppActions {
    // (Re)creates the session; returns an error message, empty on success.
    std::function<std::string(const ConnectionSettings&)> connect;
    std::function<void(bool)> show_create;
};

// One page of the client. Screens read the network and call its actions;
// they never send packets themselves. `point` is the mouse in menu coordinates.
class Screen {
public:
    virtual ~Screen() = default;
    virtual void handle(const sf::Event& event, sf::Vector2f point) = 0;
    virtual void update() {}
    virtual void draw(sf::RenderWindow& window) = 0;
};

} // namespace ntype::client
