#include <exception>
#include <iostream>

#include "ClientApp.hpp"
#include "Launcher.hpp"
#include "Options.hpp"

int main(int argc, char** argv) {
    try {
        const auto options = ntype::parse_options(argc, argv, ntype::Application::Client);
        if (options.help) {
            std::cout << "Usage: r-type_client [-n name] [-h host] [-p port]\n"
                         "Default server: 127.0.0.1:4242\n"
                         "Move your square: arrows, WASD or ZQSD\n"
                         "Pause menu: Escape | Quit: close the window\n";
            return 0;
        }
        sf::RenderWindow window;
        bool fullscreen = false;
        auto settings = ntype::client::show_launcher(
            window, fullscreen, {options.name, options.host, options.port});
        while (settings && window.isOpen()) {
            ntype::client::GameExitAction action;
            {
                ntype::client::ClientApp app(window, fullscreen, settings->name, settings->host,
                                         settings->port, settings->music_enabled);
                action = app.run();
                settings->music_enabled = app.music_enabled();
            } // Stops and destroys the level music before changing screens.
            if (action == ntype::client::GameExitAction::Quit) break;
            if (action == ntype::client::GameExitAction::Lobby)
                settings = ntype::client::show_launcher(window, fullscreen, *settings, true);
            // Restart keeps the same settings and creates a fresh server connection.
        }
    } catch (const std::exception& error) {
        std::cerr << "Client error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
