#pragma once

#include <cstdint>
#include <map>
#include <string>

#include <SFML/Graphics/RenderWindow.hpp>

#include "ClientNetwork.hpp"
#include "World.hpp"

namespace ntype::client {

class ClientApp {
public:
    ClientApp(const std::string& name, const std::string& host, std::uint16_t port);
    void run();

private:
    std::string name_;
    void poll_events();
    std::uint8_t read_input() const;
    void sync_world();
    void render();
    void create_window();
    void update_view();

    ClientNetwork network_;
    sf::RenderWindow window_;
    bool fullscreen_ = false;
    // Engine world mirroring the latest snapshot: one entity per player.
    World world_;
    std::map<std::uint16_t, int> entities_; // player id -> engine entity
};

} // namespace ntype::client
