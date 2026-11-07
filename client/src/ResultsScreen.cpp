#include "ResultsScreen.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>

#include "BackgroundConfig.hpp"
#include "GameConfig.hpp"
#include "PlayerColors.hpp"

namespace ntype::client {

namespace {

const sf::FloatRect kResultsPanel{330.f, 70.f, 620.f, 600.f};
const sf::IntRect kStarIcon{1831, 4814, 129, 123};  // FreeGUI Window.png
const sf::IntRect kTrophyIcon{200, 2060, 185, 195}; // FreeGUI Button.png

std::string padded(const std::uint32_t value, const std::size_t digits) {
    std::string text = std::to_string(value);
    if (text.size() < digits) text.insert(0, digits - text.size(), '0');
    return text;
}

} // namespace

void draw_results_screen(sf::RenderWindow& window, const GuiSkin& gui, const sf::Font& font,
                         const protocol::LobbyState& lobby, const sf::Vector2f mouse, const bool pressed) {
    const bool won = lobby.result == protocol::GameResult::Victory;

    sf::RectangleShape shade({static_cast<float>(config::kWidth), static_cast<float>(config::kHeight)});
    shade.setFillColor(sf::Color(5, 8, 30, 185));
    window.draw(shade);
    gui.panel(window, font, won ? "Victoire !" : "Game over", kResultsPanel);

    const std::string subtitle = won ? "Les trois niveaux sont termines"
                                     : "Niveau atteint : " + std::to_string(lobby.level) + " / " +
                                           std::to_string(background::kLevelCount);
    gui.centered(window, font, subtitle, {kResultsPanel.left, 128.f, kResultsPanel.width, 40.f}, 24,
                        style::muted);
    if (won) {
        for (const float x : {kResultsPanel.left + 70.f, kResultsPanel.left + kResultsPanel.width - 120.f})
            gui.icon(window, kTrophyIcon, {x, 118.f, 50.f, 53.f});
    }

    // Best score first; each name in its player's colour.
    std::vector<protocol::LobbyMember> members = lobby.members;
    std::stable_sort(members.begin(), members.end(),
                     [](const auto& a, const auto& b) { return a.score > b.score; });
    std::uint32_t total = 0;
    float y = 185.f;
    for (std::size_t rank = 0; rank < members.size(); ++rank) {
        const auto& member = members[rank];
        total += member.score;
        const sf::FloatRect row{kResultsPanel.left + 45.f, y, kResultsPanel.width - 90.f, 58.f};
        gui.surface(window, row, true);
        gui.centered(window, font, std::to_string(rank + 1), {row.left + 8.f, row.top, 40.f, row.height}, 26);
        const sf::Color colour = player_color(member.slot);
        sf::CircleShape dot(11.f);
        dot.setOrigin(11.f, 11.f);
        dot.setPosition(row.left + 66.f, row.top + row.height / 2.f);
        dot.setFillColor(colour);
        window.draw(dot);
        std::string label = member.name;
        if (member.slot == lobby.yourSlot) label += " (toi)";
        auto name = make_ui_text(window, font, label, 24);
        name.setStyle(sf::Text::Bold);
        name.setFillColor({static_cast<sf::Uint8>(colour.r * 4 / 5), static_cast<sf::Uint8>(colour.g * 4 / 5),
                           static_cast<sf::Uint8>(colour.b * 4 / 5)});
        position_ui_text(window, name, row.left + 88.f, row.top + 14.f);
        window.draw(name);
        auto score = make_ui_text(window, font, padded(member.score, 6), 26);
        score.setStyle(sf::Text::Bold);
        score.setFillColor(style::ink);
        const float score_left = row.left + row.width - 20.f - score.getGlobalBounds().width;
        position_ui_text(window, score, score_left, row.top + 12.f);
        window.draw(score);
        draw_texture_region(window, gui.windows, kStarIcon, {score_left - 42.f, row.top + 13.f, 32.f, 31.f});
        y += 66.f;
    }

    // Everyone's points together.
    const sf::FloatRect sum{kResultsPanel.left + 45.f, 455.f, kResultsPanel.width - 90.f, 90.f};
    gui.surface(window, sum);
    gui.centered(window, font, "Score total", {sum.left, sum.top + 10.f, sum.width, 28.f}, 20, style::muted);
    gui.centered(window, font, padded(total, 7), {sum.left, sum.top + 38.f, sum.width, 44.f}, 38);

    gui.button(window, font, kResultsContinue, "Continuer", mouse, pressed, true, 1, 30);
}

} // namespace ntype::client
