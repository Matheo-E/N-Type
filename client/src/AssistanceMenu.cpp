#include "AssistanceMenu.hpp"

#include "PlayerProfile.hpp"
#include "UiClickSound.hpp"

namespace ntype::client {
namespace {
const sf::FloatRect kBack{195.f, 343.f, 170.f, 43.f};
sf::FloatRect option_bounds(int row) {
    return {70.f, 90.f + static_cast<float>(row) * 70.f, 420.f, 40.f};
}
} // namespace

sf::View AssistanceMenu::make_view(const sf::RenderWindow& window) const {
    const auto size = window.getSize();
    const float width = 410.f * static_cast<float>(size.x) /
                        static_cast<float>(size.y == 0 ? 1 : size.y);
    return sf::View(sf::FloatRect((560.f - width) / 2.f, 0.f, width, 410.f));
}

void AssistanceMenu::activate(int row) {
    UiClickSound::instance().play();
    if (help || row == 3) {
        open = false;
        return;
    }
    auto& profile = PlayerProfile::shared();
    auto options = profile.accessibility();
    if (row == 0) options.reduced_motion = !options.reduced_motion;
    if (row == 1) options.no_flashes = !options.no_flashes;
    if (row == 2) options.auto_fire = !options.auto_fire;
    profile.set_accessibility(options);
}

bool AssistanceMenu::handle_event(const sf::RenderWindow& window, const sf::Event& event) {
    if (event.type == sf::Event::Closed || event.type == sf::Event::Resized ||
        (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::F11)) {
        return false;
    }
    if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Escape) {
            open = false;
        } else if (event.key.code == sf::Keyboard::Tab || event.key.code == sf::Keyboard::Down) {
            selected_ = (selected_ + 1) % 4;
        } else if (event.key.code == sf::Keyboard::Up) {
            selected_ = (selected_ + 3) % 4;
        } else if (event.key.code == sf::Keyboard::Return || event.key.code == sf::Keyboard::Space) {
            activate(selected_);
        }
    }
    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        const auto point = window.mapPixelToCoords(
            {event.mouseButton.x, event.mouseButton.y}, make_view(window));
        if (kBack.contains(point)) activate(3);
        if (!help) {
            for (int row = 0; row < 3; ++row) {
                if (option_bounds(row).contains(point)) {
                    selected_ = row;
                    activate(row);
                }
            }
        }
    }
    return true;
}

void AssistanceMenu::draw(sf::RenderWindow& window, const sf::Font& font, const GuiSkin& gui) const {
    const auto previous = window.getView();
    const auto view = make_view(window);
    window.setView(view);
    sf::RectangleShape shade(view.getSize());
    shade.setPosition(view.getCenter() - view.getSize() / 2.f);
    shade.setFillColor(sf::Color(9, 23, 44, 240));
    window.draw(shade);
    gui.panel(window, font, help ? "Comment jouer" : "Accessibilite", {35.f, 45.f, 490.f, 352.f});
    const auto mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    const bool pressed = sf::Mouse::isButtonPressed(sf::Mouse::Left);
    const auto& profile = PlayerProfile::shared();
    if (help) {
        const char* lines[] = {
            "Tu es M. Jean Cive, une dent en route chez le dentiste.",
            "Sur le chemin, des ennemis t'attaquent !",
            "Traverse la glace, les bonbons et le labo du dentiste.",
            "Esquive les tirs et les zones orange puis rouges.",
            "Tire du dentifrice et bats chaque boss pour avancer.",
            "Ramasse les objets : soins, bouclier et tirs multiples.",
            "3 points de vie. Les allies reviennent au niveau suivant.",
            "En solo ou jusqu'a 4 : protegez vos dents !"
        };
        for (int i = 0; i < 8; ++i) {
            gui.centered(window, font, lines[i], {55.f, 85.f + static_cast<float>(i) * 23.f, 450.f, 22.f}, 13);
        }
        gui.centered(window, font, "Deplacement : " + profile.binding_name(Up) + " / " + profile.binding_name(Down),
                     {55.f, 274.f, 450.f, 19.f}, 12);
        gui.centered(window, font, "Gauche / droite : " + profile.binding_name(Left) + " / " + profile.binding_name(Right),
                     {55.f, 293.f, 450.f, 19.f}, 12);
        gui.centered(window, font, "Tir : " + profile.binding_name(Shoot) + " | Pause : " + profile.binding_name(Pause),
                     {55.f, 312.f, 450.f, 19.f}, 12);
    } else {
        const auto& options = profile.accessibility();
        const bool values[] = {options.reduced_motion, options.no_flashes, options.auto_fire};
        const char* labels[] = {"Mouvements reduits", "Sans flashs d'impact", "Tir automatique"};
        const char* details[] = {"Decor fixe et aucune secousse de camera.",
                                "Supprime les flashs et clignotements des personnages.",
                                "Tire sans maintenir de bouton, hors pause et menus."};
        for (int row = 0; row < 3; ++row) {
            const auto bounds = option_bounds(row);
            gui.button(window, font, bounds, std::string(labels[row]) + (values[row] ? " : ON" : " : OFF"),
                       mouse, pressed, true, selected_ == row ? 1 : 0, 18);
            gui.centered(window, font, details[row], {60.f, bounds.top + 41.f, 440.f, 20.f}, 12);
        }
        gui.centered(window, font, profile.error().empty() ? "Tab / fleches : choisir | Entree : modifier | Echap : retour" : profile.error(),
                     {50.f, 307.f, 460.f, 26.f}, 11);
    }
    gui.button(window, font, kBack, "Retour", mouse, pressed, true, selected_ == 3 ? 1 : 2, 18);
    window.setView(previous);
}
} // namespace ntype::client
