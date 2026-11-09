#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <SFML/Window.hpp>

namespace ntype::client {

enum class ControlDevice { Keyboard, Mouse, Gamepad };
enum ControlAction { Up, Down, Left, Right, Shoot, Pause, ActionCount };

struct ControlState {
    std::array<bool, sf::Keyboard::KeyCount> keys{};
    std::array<bool, sf::Mouse::ButtonCount> mouse{};
    std::array<bool, sf::Joystick::ButtonCount> buttons{};
    float x = 0, y = 0; // mouse displacement or joystick axes; dead zone already applied
};

// Local records are per player name; bindings are shared by launcher and game.
class PlayerProfile {
public:
    explicit PlayerProfile(std::filesystem::path path);
    static PlayerProfile& shared();
    std::uint32_t best_score(const std::string& name) const;
    bool record_score(const std::string& name, std::uint32_t score);
    bool bind_key(int action, sf::Keyboard::Key key);
    bool bind_mouse(int action, sf::Mouse::Button button);
    bool bind_gamepad(int action, unsigned button);
    bool select_device(ControlDevice device);
    bool reset_controls();
    std::string binding_name(int action) const;
    std::uint8_t input(const ControlState& state) const;
    bool pause_event(const sf::Event& event) const;
    ControlDevice device() const { return device_; }
    const std::string& error() const { return error_; }

private:
    void defaults();
    void load();
    bool save();
    std::filesystem::path path_;
    std::map<std::string, std::uint32_t> scores_;
    ControlDevice device_ = ControlDevice::Keyboard;
    std::array<std::vector<int>, ActionCount> keys_;
    std::array<int, ActionCount> mouse_;
    std::array<int, ActionCount> buttons_;
    std::string error_;
};

} // namespace ntype::client
