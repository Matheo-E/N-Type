#pragma once

#include <array>
#include <optional>

#include "ClientNetwork.hpp"
#include "Screen.hpp"
#include "ui/Widgets.hpp"

namespace ntype::client {

// Lobby name and map, then Create: the creator becomes the leader.
class CreateScreen : public Screen {
public:
    CreateScreen(const ui::Theme& theme, AppActions& actions, ClientNetwork& network);
    void handle(const sf::Event& event, sf::Vector2f point) override;
    void update() override;
    void draw(sf::RenderWindow& window) override;

private:
    const ui::Theme& theme_;
    AppActions& actions_;
    ClientNetwork& network_;
    ui::TextField name_;
    std::array<sf::FloatRect, config::kMapCount> maps_;
    std::uint8_t map_ = 0;
    ui::Button create_;
    ui::Button back_;
};

// Draws the selectable map cards (also used by the waiting room).
void draw_map_cards(sf::RenderTarget& target, const ui::Theme& theme,
                    const std::array<sf::FloatRect, config::kMapCount>& cards, std::uint8_t selected);
// The card under `point`, if any.
std::optional<std::uint8_t> map_card_at(const std::array<sf::FloatRect, config::kMapCount>& cards,
                                        const sf::Event& event, sf::Vector2f point);

} // namespace ntype::client
