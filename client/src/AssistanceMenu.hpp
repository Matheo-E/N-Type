#pragma once

#include "GuiSkin.hpp"

namespace ntype::client {

// Shared by the launcher and pause menu; all settings apply immediately.
class AssistanceMenu {
public:
    bool open = false;
    bool help = false;
    bool handle_event(const sf::RenderWindow& window, const sf::Event& event);
    void draw(sf::RenderWindow& window, const sf::Font& font, const GuiSkin& gui) const;

private:
    int selected_ = 0;
    sf::View make_view(const sf::RenderWindow& window) const;
    void activate(int row);
};

} // namespace ntype::client
