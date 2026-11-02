#include "ConnectScreen.hpp"

#include <algorithm>
#include <charconv>

#include "Protocol.hpp"

namespace ntype::client {

namespace {

constexpr std::array<const char*, 3> kLabels{"Name", "Host", "Port"};

} // namespace

ConnectScreen::ConnectScreen(const ui::Theme& theme, AppActions& actions, const ConnectionSettings& defaults,
                             const std::unique_ptr<ClientNetwork>& network)
    : theme_(theme), actions_(actions), network_(network),
      fields_{{{{500, 220, 390, 52}, defaults.name, protocol::kMaxPlayerName},
               {{500, 300, 390, 52}, defaults.host, 40},
               {{500, 380, 390, 52}, std::to_string(defaults.port), 5}}},
      continue_{"Continue", {500, 470, 390, 56}} {
    fields_[0].focused = true;
}

void ConnectScreen::submit() {
    const auto& port_text = fields_[2].value;
    unsigned port = 0;
    const auto result = std::from_chars(port_text.data(), port_text.data() + port_text.size(), port);
    if (fields_[0].value.empty() || fields_[1].value.empty() || result.ec != std::errc{} ||
        result.ptr != port_text.data() + port_text.size() || port == 0 || port > 65535) {
        error_ = "Please enter a name, host and a valid port.";
        return;
    }
    error_ = actions_.connect({fields_[0].value, fields_[1].value, static_cast<std::uint16_t>(port)});
}

void ConnectScreen::handle(const sf::Event& event, const sf::Vector2f point) {
    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Tab) {
        const auto focused = std::find_if(fields_.begin(), fields_.end(), [](const auto& f) { return f.focused; });
        const auto next = focused == fields_.end() ? 0 : (focused - fields_.begin() + 1) % 3;
        for (auto& field : fields_) {
            field.focused = false;
        }
        fields_[static_cast<std::size_t>(next)].focused = true;
        return;
    }
    bool submitted = continue_.handle(event, point);
    for (auto& field : fields_) {
        submitted = field.handle(event, point) || submitted;
    }
    if (submitted) {
        submit();
    }
}

void ConnectScreen::draw(sf::RenderWindow& window) {
    ui::draw_label(window, theme_, "N-Type", {ui::kMenuWidth / 2.0F, 110}, theme_.title_size + 16, theme_.text, true);
    ui::draw_label(window, theme_, "Connect to a server", {ui::kMenuWidth / 2.0F, 165}, theme_.text_size,
                   theme_.muted, true);
    for (std::size_t i = 0; i < fields_.size(); ++i) {
        ui::draw_label(window, theme_, kLabels[i], {390, fields_[i].bounds.top + 12}, theme_.text_size, theme_.text);
        fields_[i].draw(window, theme_);
    }
    continue_.draw(window, theme_);
    if (!error_.empty()) {
        ui::draw_label(window, theme_, error_, {ui::kMenuWidth / 2.0F, 570}, theme_.small_size, theme_.error, true);
    } else if (network_) {
        ui::draw_label(window, theme_, network_->status(), {ui::kMenuWidth / 2.0F, 570}, theme_.small_size,
                       theme_.muted, true);
    }
    ui::draw_label(window, theme_, "Tab: next field   Enter: continue   F11: fullscreen   Esc: quit",
                   {ui::kMenuWidth / 2.0F, 680}, theme_.small_size, theme_.muted, true);
}

} // namespace ntype::client
