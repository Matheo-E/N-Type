#include "WaitingRoomScreen.hpp"

#include <SFML/Graphics/RectangleShape.hpp>

namespace ntype::client {

WaitingRoomScreen::WaitingRoomScreen(const ui::Theme& theme, ClientNetwork& network)
    : theme_(theme), network_(network), name_{{760, 290, 320, 52}, "", protocol::kMaxLobbyName},
      rename_{"Rename", {1090, 290, 110, 52}, ui::Style::Secondary},
      start_{"Start", {760, 594, 440, 56}}, leave_{"Leave", {80, 594, 240, 56}, ui::Style::Danger} {}

void WaitingRoomScreen::rename() {
    const auto& lobby = network_.lobby();
    if (lobby && !name_.value.empty() && name_.value != lobby->name) {
        network_.rename(name_.value);
    }
}

void WaitingRoomScreen::handle(const sf::Event& event, const sf::Vector2f point) {
    if (leave_.handle(event, point)) {
        network_.leave();
        return;
    }
    if (!leader_) {
        return;
    }
    if (name_.handle(event, point) || rename_.handle(event, point)) {
        rename();
    }
    if (start_.handle(event, point)) {
        network_.start();
    }
}

void WaitingRoomScreen::update() {
    const auto& lobby = network_.lobby();
    if (!lobby) {
        return;
    }
    // Promoted (or created the lobby): the leader's controls start from the current name.
    const bool leader = network_.is_leader();
    if (leader && !leader_) {
        name_.value = lobby->name;
    }
    leader_ = leader;
    rename_.enabled = !network_.busy() && !name_.value.empty() && name_.value != lobby->name;
    start_.enabled = !network_.busy();
}

void WaitingRoomScreen::draw(sf::RenderWindow& window) {
    const auto& lobby = network_.lobby();
    if (!lobby) {
        return;
    }
    ui::draw_label(window, theme_, lobby->name, {80, 40}, theme_.title_size, theme_.text);
    ui::draw_label(window, theme_, "Room code", {82, 112}, theme_.text_size, theme_.muted);
    ui::draw_label(window, theme_, lobby->code, {80, 140}, theme_.title_size + 32, theme_.text);

    ui::draw_label(window, theme_, "Players " + std::to_string(lobby->members.size()) + "/4", {82, 250},
                   theme_.text_size, theme_.muted);
    for (std::size_t i = 0; i < lobby->members.size(); ++i) {
        const auto& member = lobby->members[i];
        const sf::FloatRect row{80, 290 + static_cast<float>(i) * 64.0F, 600, 56};
        ui::draw_panel(window, theme_, row);
        sf::RectangleShape square({36, 36});
        square.setPosition(row.left + 10, row.top + 10);
        square.setFillColor(theme_.players[member.slot - 1U]);
        window.draw(square);
        std::string label = member.name;
        if (member.slot == lobby->yourSlot) {
            label += " (you)";
        }
        ui::draw_label(window, theme_, label, {row.left + 62, row.top + 12}, theme_.text_size, theme_.text);
        std::string tags;
        if (member.slot == lobby->leaderSlot) {
            tags = "leader";
        }
        if (member.role == protocol::MemberRole::Spectator) {
            tags += tags.empty() ? "spectator" : "  spectator";
        }
        ui::draw_label(window, theme_, tags, {row.left + 470, row.top + 16}, theme_.small_size, theme_.muted);
    }

    if (leader_) {
        ui::draw_label(window, theme_, "Lobby name", {760, 250}, theme_.text_size, theme_.text);
        name_.draw(window, theme_);
        rename_.draw(window, theme_);
        start_.draw(window, theme_);
    } else {
        ui::draw_label(window, theme_, "Waiting for the leader to start...", {980, 310}, theme_.text_size,
                       theme_.muted, true);
    }
    leave_.draw(window, theme_);
    if (!network_.error().empty()) {
        ui::draw_label(window, theme_, network_.error(), {340, 610}, theme_.small_size, theme_.error);
    }
    ui::draw_label(window, theme_, "Esc: leave the lobby", {ui::kMenuWidth / 2.0F, 690}, theme_.small_size,
                   theme_.muted, true);
}

} // namespace ntype::client
