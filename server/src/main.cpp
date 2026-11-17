#include <atomic>
#include <csignal>
#include <exception>
#include <iostream>

#include "Options.hpp"
#include "Server.hpp"

namespace {

std::atomic<bool> running{true};

void on_signal(int) { running = false; }

} // namespace

int main(int argc, char** argv) {
    try {
        const ntype::Options options = ntype::parse_options(argc, argv, ntype::Application::Server);
        if (options.help) {
            std::cout << "USAGE: ./r-type_server [-p port] [--no-enemies]\n"
                         "  -p port        UDP port to listen on (default 4242)\n"
                         "  --no-enemies   games have no enemies, only ships (for tests)\n"
                         "Console commands: level <code>, gameover <code>, score <code> <slot> <points>\n";
            return 0;
        }
        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);
        ntype::server::Server server(options.port, options.no_enemies);
#ifndef _WIN32
        // Started in the background (`&`), reading the terminal would stop the whole
        // server; ignoring SIGTTIN makes the read fail and the console thread end.
        std::signal(SIGTTIN, SIG_IGN);
#endif
        ntype::server::Console console;
        server.set_console(&console);
        server.run(running);
    } catch (const std::exception& error) {
        std::cerr << "[server] fatal: " << error.what() << '\n';
        return 84;
    }
    return 0;
}
