#pragma once

#include <array>
#include <memory>

#include "ClientNetwork.hpp"
#include "Screen.hpp"
#include "ui/Widgets.hpp"

namespace ntype::client {

// Name, host and port, then Continue. Shows the connection status meanwhile.
class ConnectScreen : public Screen {
public:
    ConnectScreen(const ui::Theme& theme, AppActions& actions, const ConnectionSettings& defaults,
                  const std::unique_ptr<ClientNetwork>& network);
    void handle(const sf::Event& event, sf::Vector2f point) override;
    void draw(sf::RenderWindow& window) override;

private:
    void submit();

    const ui::Theme& theme_;
    AppActions& actions_;
    const std::unique_ptr<ClientNetwork>& network_;
    std::array<ui::TextField, 3> fields_; // name, host, port
    ui::Button continue_;
    std::string error_;
};

} // namespace ntype::client
