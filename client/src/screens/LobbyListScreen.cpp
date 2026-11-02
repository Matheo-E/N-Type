#include "LobbyListScreen.hpp"

#include <array>


namespace ntype::client {

namespace {

constexpr std::array<const char*, 4> kHeaders{"Lobby", "Code", "Players", "State"};

} // namespace

LobbyListScreen::LobbyListScreen(const ui::Theme& theme, AppActions& actions, ClientNetwork& network)
    : theme_(theme), actions_(actions), network_(network), list_{{80, 150, 760, 432}},
      join_selected_{"Join selected", {880, 150, 320, 56}},
      code_{{880, 280, 200, 56}, "", protocol::kCodeLength, ui::Filter::Code},
      join_code_{"Join", {1090, 280, 110, 56}},
      create_{"Create lobby", {880, 400, 320, 56}, ui::Style::Secondary} {
    list_.columns = {0, 400, 520, 640};
}

void LobbyListScreen::join_selected() {
    if (!selected_code_.empty()) {
        network_.join(selected_code_);
    }
}

void LobbyListScreen::handle(const sf::Event& event, const sf::Vector2f point) {
    const auto list_event = list_.handle(event, point);
    if (list_.selected && *list_.selected < codes_.size()) {
        selected_code_ = codes_[*list_.selected];
    }
    if (list_event == ui::ListEvent::Activated || join_selected_.handle(event, point)) {
        join_selected();
    }
    if (code_.handle(event, point) || join_code_.handle(event, point)) {
        if (code_.value.size() == protocol::kCodeLength) {
            network_.join(code_.value);
        }
    }
    if (create_.handle(event, point)) {
        network_.clear_error();
        actions_.show_create(true);
    }
}

void LobbyListScreen::update() {
    const auto& lobbies = network_.lobbies();
    list_.rows.clear();
    codes_.clear();
    list_.selected.reset();
    for (const auto& lobby : lobbies) {
        if (lobby.code == selected_code_) {
            list_.selected = list_.rows.size();
        }
        list_.rows.push_back({lobby.name, lobby.code, std::to_string(lobby.members) + "/4",
                              lobby.state == protocol::LobbyPhase::Playing ? "Playing" : "Waiting"});
        codes_.push_back(lobby.code);
    }
    if (!list_.selected) {
        selected_code_.clear();
    }
    if (list_.scroll > 0 && list_.scroll + list_.visible_rows() > list_.rows.size()) {
        list_.scroll = list_.rows.size() > list_.visible_rows() ? list_.rows.size() - list_.visible_rows() : 0;
    }
    join_selected_.enabled = list_.selected.has_value() && !network_.busy();
    join_code_.enabled = code_.value.size() == protocol::kCodeLength && !network_.busy();
    create_.enabled = !network_.busy();
}

void LobbyListScreen::draw(sf::RenderWindow& window) {
    ui::draw_label(window, theme_, "Lobbies", {80, 40}, theme_.title_size, theme_.text);
    ui::draw_label(window, theme_, "Playing as " + network_.name(), {82, 92}, theme_.small_size, theme_.muted);
    for (std::size_t i = 0; i < kHeaders.size(); ++i) {
        ui::draw_label(window, theme_, kHeaders[i], {80 + theme_.padding + list_.columns[i], 120}, theme_.small_size,
                       theme_.muted);
    }
    if (list_.rows.empty()) {
        ui::draw_label(window, theme_, "No lobby yet - create one!", {460, 360}, theme_.text_size, theme_.muted, true);
    }
    list_.draw(window, theme_);
    join_selected_.draw(window, theme_);
    ui::draw_label(window, theme_, "Room code", {880, 245}, theme_.text_size, theme_.text);
    code_.draw(window, theme_);
    join_code_.draw(window, theme_);
    create_.draw(window, theme_);
    if (!network_.error().empty()) {
        ui::draw_label(window, theme_, network_.error(), {880, 480}, theme_.small_size, theme_.error);
    }
    ui::draw_label(window, theme_, "Double-click a lobby to join   Esc: disconnect", {ui::kMenuWidth / 2.0F, 680},
                   theme_.small_size, theme_.muted, true);
}

} // namespace ntype::client
