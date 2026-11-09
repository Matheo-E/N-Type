#include "client/src/PlayerProfile.hpp"
#include "Protocol.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace ntype::client;

int main() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("ntype-profile-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path = directory / "profile.json";
    {
        PlayerProfile profile(path);
        assert(profile.best_score("Ann") == 0);
        assert(profile.record_score("Ann", 500));
        assert(profile.record_score("Ann", 100));
        assert(profile.record_score("Bob", 200));
        ControlState state;
        state.keys[sf::Keyboard::Z] = true;
        assert(profile.input(state) == static_cast<std::uint8_t>(protocol::InputType::Up));
        assert(profile.bind_key(Up, sf::Keyboard::T));
        assert(profile.input(state) == 0); // old aliases no longer move the player
        state.keys[sf::Keyboard::T] = true;
        assert(profile.input(state) == 1);
        assert(profile.bind_key(Shoot, sf::Keyboard::T)); // conflict swaps the two actions
        assert(profile.input(state) == 16);
        assert(profile.bind_key(Up, sf::Keyboard::I));
        assert(!profile.bind_key(Up, sf::Keyboard::F11));
        assert(profile.bind_key(Pause, sf::Keyboard::P));
        sf::Event pause{};
        pause.type = sf::Event::KeyPressed; pause.key.code = sf::Keyboard::P;
        assert(profile.pause_event(pause));
        pause.key.code = sf::Keyboard::Escape;
        assert(!profile.pause_event(pause));
    }
    {
        PlayerProfile profile(path); // a fresh process sees both records and bindings
        assert(profile.best_score("Ann") == 500 && profile.best_score("Bob") == 200);
        ControlState state;
        state.keys[sf::Keyboard::I] = true;
        state.keys[sf::Keyboard::T] = true;
        assert(profile.input(state) == 17);
        assert(profile.select_device(ControlDevice::Mouse));
        state.x = 50; state.y = -50;
        state.mouse[sf::Mouse::Left] = true;
        assert(profile.input(state) == (1 | 8 | 16));
        assert(profile.bind_mouse(Shoot, sf::Mouse::Middle));
        assert(profile.input(state) == (1 | 8));
        state.mouse[sf::Mouse::Middle] = true;
        assert(profile.input(state) == (1 | 8 | 16));
        assert(profile.select_device(ControlDevice::Gamepad));
        assert(profile.bind_gamepad(Shoot, 2));
        state = {}; state.x = -80; state.buttons[2] = true;
        assert(profile.input(state) == (4 | 16));
        sf::Event pause{}; pause.type = sf::Event::JoystickButtonPressed; pause.joystickButton.button = 7;
        assert(profile.pause_event(pause));
    }
    {
        PlayerProfile profile(path);
        assert(profile.device() == ControlDevice::Gamepad);
        ControlState state; state.buttons[2] = true;
        assert(profile.input(state) == 16);
        assert(profile.reset_controls());
        assert(profile.device() == ControlDevice::Keyboard);
        assert(profile.best_score("Ann") == 500); // restoring controls keeps scores
    }
    {
        std::ofstream(path) << "broken JSON";
        PlayerProfile profile(path);
        ControlState state; state.keys[sf::Keyboard::Space] = true;
        assert(profile.input(state) == 16); // a damaged file does not prevent playing
    }
    {
        PlayerProfile profile(path / "cannot-write-inside-file.json");
        assert(!profile.record_score("Ann", 20));
        assert(!profile.error().empty());
    }
    std::filesystem::remove_all(directory);
    std::cout << "Player profile tests passed!\n";
}
