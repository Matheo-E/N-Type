#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

#include "ClientNetwork.hpp"

namespace ntype::client {

class ClientApp {
public:
    ClientApp(const std::string& host, std::uint16_t port);
    void run();

private:
    void poll_events();
    std::uint8_t read_input() const;
    void render();

    ClientNetwork network_;
    sf::RenderWindow window_;
    std::array<sf::RectangleShape, 2> squares_;
};

} // namespace ntype::client
