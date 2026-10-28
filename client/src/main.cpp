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
                         "Quit: Escape or close the window\n";
            return 0;
        }
        const auto settings = ntype::client::show_launcher({options.name, options.host, options.port});
        if (!settings) return 0;
        ntype::client::ClientApp app(settings->name, settings->host, settings->port);
        app.run();
    } catch (const std::exception& error) {
        std::cerr << "Client error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
