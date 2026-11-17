#pragma once

#include "GuiSkin.hpp"

namespace ntype::client {

/** Edits the persistent bindings used immediately by gameplay. */
class ControlsMenu {
public:
    bool open = false;

    bool handle_event(const sf::RenderWindow& window, const sf::Event& event);
    void draw(sf::RenderWindow& window, const sf::Font& font, const GuiSkin& gui) const;

private:
    int selected_action_ = -1;

    sf::View make_view(const sf::RenderWindow& window) const;
};

} // namespace ntype::client
