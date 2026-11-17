#pragma once

#include "GuiSkin.hpp"

namespace ntype::client {

/** Visual control settings only. Gameplay bindings are not changed here. */
class ControlsMenu {
public:
    bool open = false;

    bool handle_event(const sf::RenderWindow& window, const sf::Event& event);
    void draw(sf::RenderWindow& window, const sf::Font& font, const GuiSkin& gui) const;

private:
    enum class Device { Keyboard, Mouse, Gamepad };
    Device device_ = Device::Keyboard;
    int selected_action_ = -1;

    sf::View make_view(const sf::RenderWindow& window) const;
};

} // namespace ntype::client
