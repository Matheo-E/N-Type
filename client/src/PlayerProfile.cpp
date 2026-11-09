#include "PlayerProfile.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>

#include "Protocol.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace ntype::client {
namespace {
std::filesystem::path profile_path() {
    if (const char* path = std::getenv("NTYPE_PROFILE_PATH")) return path;
#ifdef _WIN32
    if (const char* path = std::getenv("APPDATA")) return std::filesystem::path(path) / "N-Type/client.json";
#else
    if (const char* path = std::getenv("XDG_CONFIG_HOME")) return std::filesystem::path(path) / "n-type/client.json";
    if (const char* path = std::getenv("HOME")) return std::filesystem::path(path) / ".config/n-type/client.json";
#endif
    return "n-type-client.json";
}

std::string key_name(int key) {
    if (key >= sf::Keyboard::A && key <= sf::Keyboard::Z) return std::string(1, static_cast<char>('A' + key));
    if (key >= sf::Keyboard::Num0 && key <= sf::Keyboard::Num9) return std::to_string(key - sf::Keyboard::Num0);
    if (key >= sf::Keyboard::F1 && key <= sf::Keyboard::F15) return "F" + std::to_string(key - sf::Keyboard::F1 + 1);
    switch (key) {
    case sf::Keyboard::Up: return "Haut";
    case sf::Keyboard::Down: return "Bas";
    case sf::Keyboard::Left: return "Gauche";
    case sf::Keyboard::Right: return "Droite";
    case sf::Keyboard::Space: return "Espace";
    case sf::Keyboard::Escape: return "Echap";
    case sf::Keyboard::Return: return "Entree";
    case sf::Keyboard::Tab: return "Tab";
    case sf::Keyboard::BackSpace: return "Retour arriere";
    case sf::Keyboard::LShift: return "Maj gauche";
    case sf::Keyboard::RShift: return "Maj droite";
    case sf::Keyboard::LControl: return "Ctrl gauche";
    case sf::Keyboard::RControl: return "Ctrl droite";
    case sf::Keyboard::LAlt: return "Alt gauche";
    case sf::Keyboard::RAlt: return "Alt droite";
    default: return "Touche " + std::to_string(key);
    }
}

void swap_button(std::array<int, ActionCount>& bindings, int action, int button) {
    for (int other = 0; other < ActionCount; ++other) {
        if (other != action && bindings[other] == button) bindings[other] = bindings[action];
    }
    bindings[action] = button;
}
} // namespace

PlayerProfile::PlayerProfile(std::filesystem::path path) : path_(std::move(path)) { defaults(); load(); }
PlayerProfile& PlayerProfile::shared() { static PlayerProfile profile(profile_path()); return profile; }

void PlayerProfile::defaults() {
    device_ = ControlDevice::Keyboard;
    keys_ = {{{sf::Keyboard::Z, sf::Keyboard::W, sf::Keyboard::Up},
              {sf::Keyboard::S, sf::Keyboard::Down}, {sf::Keyboard::Q, sf::Keyboard::A, sf::Keyboard::Left},
              {sf::Keyboard::D, sf::Keyboard::Right}, {sf::Keyboard::Space}, {sf::Keyboard::Escape}}};
    mouse_ = {{-1, -1, -1, -1, sf::Mouse::Left, sf::Mouse::Right}};
    buttons_ = {{-1, -1, -1, -1, 0, 7}};
}

void PlayerProfile::load() {
    std::ifstream file(path_);
    if (!file) return;
    try {
        const auto json = nlohmann::json::parse(file);
        if (json.contains("best_scores") && json["best_scores"].is_object()) {
            for (auto it = json["best_scores"].begin(); it != json["best_scores"].end(); ++it) {
                if (it.value().is_number_unsigned() && it.value().get<std::uint64_t>() <= UINT32_MAX)
                    scores_[it.key()] = it.value().get<std::uint32_t>();
            }
        }
        const auto& controls = json.at("controls");
        const int device = controls.at("device").get<int>();
        auto keys = controls.at("keyboard").get<decltype(keys_)>();
        auto mouse = controls.at("mouse").get<decltype(mouse_)>();
        auto buttons = controls.at("gamepad").get<decltype(buttons_)>();
        if (device < 0 || device > 2) return;
        for (const auto& action : keys) for (int key : action)
            if (key < 0 || key >= sf::Keyboard::KeyCount || key == sf::Keyboard::F11) return;
        for (int button : mouse) if (button < -1 || button >= sf::Mouse::ButtonCount) return;
        for (int button : buttons) if (button < -1 || button >= sf::Joystick::ButtonCount) return;
        device_ = static_cast<ControlDevice>(device);
        keys_ = std::move(keys); mouse_ = mouse; buttons_ = buttons;
    } catch (const nlohmann::json::exception&) {
        error_ = "Profil illisible : controles par defaut.";
    }
}

bool PlayerProfile::save() {
    try {
        if (!path_.parent_path().empty()) std::filesystem::create_directories(path_.parent_path());
        auto temporary = path_; temporary += ".tmp";
        const nlohmann::json json = {{"best_scores", scores_}, {"controls", {
            {"device", static_cast<int>(device_)}, {"keyboard", keys_}, {"mouse", mouse_}, {"gamepad", buttons_}}}};
        std::ofstream file(temporary, std::ios::trunc);
        file << json.dump(2) << '\n';
        file.close();
        if (!file) throw std::runtime_error("profile write failed");
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("profile replacement failed");
#else
        std::filesystem::rename(temporary, path_);
#endif
        error_.clear();
        return true;
    } catch (const std::exception&) {
        error_ = "Sauvegarde impossible : verifie les droits du dossier.";
        return false;
    }
}

std::uint32_t PlayerProfile::best_score(const std::string& name) const {
    const auto it = scores_.find(name); return it == scores_.end() ? 0 : it->second;
}
bool PlayerProfile::record_score(const std::string& name, std::uint32_t score) {
    if (name.empty()) return true;
    if (score <= best_score(name)) return error_.empty() || save();
    scores_[name] = score; return save();
}
bool PlayerProfile::bind_key(int action, sf::Keyboard::Key key) {
    if (action < 0 || action >= ActionCount || key < 0 || key >= sf::Keyboard::KeyCount || key == sf::Keyboard::F11) return false;
    for (int other = 0; other < ActionCount; ++other) {
        if (other != action && std::find(keys_[other].begin(), keys_[other].end(), key) != keys_[other].end())
            keys_[other] = keys_[action];
    }
    keys_[action] = {key}; return save();
}
bool PlayerProfile::bind_mouse(int action, sf::Mouse::Button button) {
    if (action < 0 || action >= ActionCount || button < 0 || button >= sf::Mouse::ButtonCount) return false;
    swap_button(mouse_, action, button); return save();
}
bool PlayerProfile::bind_gamepad(int action, unsigned button) {
    if (action < 0 || action >= ActionCount || button >= sf::Joystick::ButtonCount) return false;
    swap_button(buttons_, action, static_cast<int>(button)); return save();
}
bool PlayerProfile::select_device(ControlDevice device) { device_ = device; return save(); }
bool PlayerProfile::reset_controls() { defaults(); return save(); }

std::string PlayerProfile::binding_name(int action) const {
    if (device_ == ControlDevice::Keyboard) {
        std::string label;
        for (int key : keys_[action]) { if (!label.empty()) label += " / "; label += key_name(key); }
        return label.empty() ? "--" : label;
    }
    const int button = device_ == ControlDevice::Mouse ? mouse_[action] : buttons_[action];
    std::string label;
    if (action < Shoot) label = device_ == ControlDevice::Mouse ? "Curseur" : "Stick";
    if (button >= 0) {
        if (!label.empty()) label += " / ";
        if (device_ == ControlDevice::Mouse) {
            const char* names[] = {"Clic gauche", "Clic droit", "Clic milieu", "Bouton 4", "Bouton 5"};
            label += names[button];
        } else label += "Bouton " + std::to_string(button + 1);
    }
    return label.empty() ? "--" : label;
}

std::uint8_t PlayerProfile::input(const ControlState& state) const {
    std::uint8_t bits = 0;
    for (int action = Up; action <= Shoot; ++action) {
        bool pressed = false;
        if (device_ == ControlDevice::Keyboard) {
            for (int key : keys_[action]) pressed = pressed || state.keys[static_cast<std::size_t>(key)];
        } else {
            const auto& bindings = device_ == ControlDevice::Mouse ? mouse_ : buttons_;
            const int button = bindings[action];
            if (button >= 0) pressed = device_ == ControlDevice::Mouse ? state.mouse[button] : state.buttons[button];
            if (action == Up) pressed = pressed || state.y < 0;
            if (action == Down) pressed = pressed || state.y > 0;
            if (action == Left) pressed = pressed || state.x < 0;
            if (action == Right) pressed = pressed || state.x > 0;
        }
        if (pressed) bits |= static_cast<std::uint8_t>(1u << action);
    }
    return bits;
}

bool PlayerProfile::pause_event(const sf::Event& event) const {
    if (event.type == sf::Event::KeyPressed)
        return std::find(keys_[Pause].begin(), keys_[Pause].end(), event.key.code) != keys_[Pause].end();
    if (device_ == ControlDevice::Mouse && event.type == sf::Event::MouseButtonPressed)
        return event.mouseButton.button == mouse_[Pause];
    if (device_ == ControlDevice::Gamepad && event.type == sf::Event::JoystickButtonPressed)
        return static_cast<int>(event.joystickButton.button) == buttons_[Pause];
    return false;
}
} // namespace ntype::client
