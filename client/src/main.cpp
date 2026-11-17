#include <exception>
#include <iostream>
#include <memory>

#include "ClientApp.hpp"
#include "Launcher.hpp"
#include "Options.hpp"
#include "UiClickSound.hpp"

int main(int argc, char** argv) {
    using namespace ntype::client;
    try {
        const auto options = ntype::parse_options(argc, argv, ntype::Application::Client);
        if (options.help) {
            std::cout << "Usage: r-type_client [-n name] [-h host] [-p port]\n"
                         "Default server: 127.0.0.1:4242\n"
                         "Create or join a lobby, then move your tooth: arrows, WASD or ZQSD\n"
                         "Pause menu: Escape | Quit: close the window\n";
            return 0;
        }
        sf::RenderWindow window;
        WindowState window_state;
        ConnectionSettings settings{options.name, options.host, options.port};
        // One session from the connection page to the game and back: lobby
        // membership belongs to it.
        std::unique_ptr<ClientNetwork> session;
        LauncherStart start = LauncherStart::Title;
        // The launcher opens the window itself, so it runs before any isOpen() check.
        while (show_launcher(window, window_state, settings, session, start)) {
            GameExitAction action;
            {
                ClientApp app(window, window_state, *session, settings.music_enabled);
                action = app.run();
                settings.music_enabled = app.music_enabled();
                settings.click_sound_enabled = UiClickSound::instance().enabled();
            } // Stops and destroys the level music before changing screens.
            if (action == GameExitAction::Quit) break;
            start = action == GameExitAction::WaitingRoom ? LauncherStart::WaitingRoom
                                                          : LauncherStart::Lobbies;
        }
    } catch (const std::exception& error) {
        std::cerr << "Client error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
