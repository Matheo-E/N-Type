#include "ControlsMenu.hpp"

#include "UiClickSound.hpp"
#include "PlayerProfile.hpp"

namespace ntype::client {
namespace {

const sf::FloatRect kTabs[] = {
    {55.f, 86.f, 145.f, 43.f},
    {207.f, 86.f, 145.f, 43.f},
    {359.f, 86.f, 145.f, 43.f}
};
const sf::FloatRect kBackButton{315.f, 343.f, 140.f, 43.f};
const sf::FloatRect kResetButton{105.f, 343.f, 190.f, 43.f};
const char* const kActions[] = {"Monter", "Descendre", "Gauche", "Droite", "Tirer", "Pause"};

sf::FloatRect binding_bounds(int row) {
    return {280.f, 143.f + static_cast<float>(row) * 27.f, 209.f, 25.f};
}

} // namespace

sf::View ControlsMenu::make_view(const sf::RenderWindow& window) const {
    const sf::Vector2u size = window.getSize();
    const float width = 410.f * static_cast<float>(size.x) /
                        static_cast<float>(size.y == 0 ? 1 : size.y);
    return sf::View(sf::FloatRect((560.f - width) / 2.f, 0.f, width, 410.f));
}

bool ControlsMenu::handle_event(const sf::RenderWindow& window, const sf::Event& event) {
    if (event.type == sf::Event::Closed || event.type == sf::Event::Resized ||
        (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::F11)) {
        return false;
    }
    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
        UiClickSound::instance().play();
        if (selected_action_ >= 0) selected_action_ = -1;
        else open = false;
        return true;
    }
    auto& profile = PlayerProfile::shared();
    if (selected_action_ >= 0) {
        bool captured = false;
        if (profile.device() == ControlDevice::Keyboard && event.type == sf::Event::KeyPressed &&
            event.key.code != sf::Keyboard::Unknown) {
            profile.bind_key(selected_action_, event.key.code);
            captured = true;
        } else if (profile.device() == ControlDevice::Mouse && event.type == sf::Event::MouseButtonPressed) {
            profile.bind_mouse(selected_action_, event.mouseButton.button);
            captured = true;
        } else if (profile.device() == ControlDevice::Gamepad && event.type == sf::Event::JoystickButtonPressed) {
            profile.bind_gamepad(selected_action_, event.joystickButton.button);
            captured = true;
        }
        if (captured) {
            selected_action_ = -1;
            UiClickSound::instance().play();
        }
        return true;
    }
    if (event.type != sf::Event::MouseButtonPressed || event.mouseButton.button != sf::Mouse::Left) {
        return true;
    }
    const sf::Vector2f point = window.mapPixelToCoords(
        {event.mouseButton.x, event.mouseButton.y}, make_view(window));
    if (kBackButton.contains(point)) {
        UiClickSound::instance().play();
        open = false;
        return true;
    }
    if (kResetButton.contains(point)) {
        profile.reset_controls();
        UiClickSound::instance().play();
        return true;
    }
    for (int tab = 0; tab < 3; ++tab) {
        if (kTabs[tab].contains(point)) {
            UiClickSound::instance().play();
            profile.select_device(static_cast<ControlDevice>(tab));
            selected_action_ = -1;
            return true;
        }
    }
    for (int row = 0; row < 6; ++row) {
        if (binding_bounds(row).contains(point)) {
            UiClickSound::instance().play();
            selected_action_ = row;
            return true;
        }
    }
    return true;
}

void ControlsMenu::draw(sf::RenderWindow& window, const sf::Font& font, const GuiSkin& gui) const {
    const auto& profile = PlayerProfile::shared();
    const sf::View previous_view = window.getView();
    const sf::View view = make_view(window);
    window.setView(view);
    sf::RectangleShape shade(view.getSize());
    shade.setPosition(view.getCenter() - view.getSize() / 2.f);
    shade.setFillColor(sf::Color(9, 23, 44, 220));
    window.draw(shade);
    gui.panel(window, font, "Controles", {35.f, 57.f, 490.f, 340.f});

    const char* const tabs[] = {"Clavier", "Souris", "Manette"};
    const sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    const bool pressed = sf::Mouse::isButtonPressed(sf::Mouse::Left);
    for (int tab = 0; tab < 3; ++tab) {
        gui.button(window, font, kTabs[tab], tabs[tab], mouse, pressed,
                   true, static_cast<int>(profile.device()) == tab ? 1 : 0, 16);
    }
    for (int row = 0; row < 6; ++row) {
        const sf::FloatRect bounds = binding_bounds(row);
        gui.surface(window, {65.f, bounds.top, 424.f, bounds.height}, true);
        gui.centered(window, font, kActions[row], {75.f, bounds.top, 175.f, bounds.height}, 13);
        gui.button(window, font, bounds, selected_action_ == row ? "Appuie... (Echap annule)" : profile.binding_name(row), mouse, pressed,
                   true, selected_action_ == row ? 1 : 0, 12);
    }
    gui.centered(window, font, profile.error().empty() ? "Mode actif : " + std::string(tabs[static_cast<int>(profile.device())]) : profile.error(),
                 {55.f, 310.f, 450.f, 20.f}, 13, style::muted);
    gui.centered(window, font, "Clique une commande puis appuie. F11 reserve au plein ecran.",
                 {55.f, 329.f, 450.f, 13.f}, 10, style::muted);
    gui.button(window, font, kResetButton, "Par defaut", mouse, pressed, true, 0, 16);
    gui.button(window, font, kBackButton, "Retour", mouse, pressed, true, 2, 16);
    window.setView(previous_view);
}

} // namespace ntype::client
