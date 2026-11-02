#include <exception>
#include <iostream>

#include "ClientApp.hpp"
#include "Options.hpp"

int main(int argc, char** argv) {
    try {
        const auto options = ntype::parse_options(argc, argv, ntype::Application::Client);
        if (options.help) {
            std::cout << "Usage: r-type_client [-n name] [-h host] [-p port]\n"
                         "Default server: 127.0.0.1:4242\n"
                         "Create or join a lobby, then move your square: arrows, WASD or ZQSD\n"
                         "Esc: back / leave the lobby, F11: fullscreen, close the window to quit\n";
            return 0;
        }
        ntype::client::ClientApp app({options.name, options.host, options.port});
        app.run();
    } catch (const std::exception& error) {
        std::cerr << "Client error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
