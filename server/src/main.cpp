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
        const auto options = ntype::parse_options(argc, argv, ntype::Application::Server);
        if (options.help) {
            std::cout << "USAGE: ./r-type_server [-p port]\n"
                         "  -p port   UDP port to listen on (default 4242)\n";
            return 0;
        }
        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);
        ntype::server::Server server(options.port);
        server.run(running);
    } catch (const std::exception& error) {
        std::cerr << "[server] fatal: " << error.what() << '\n';
        return 84;
    }
    return 0;
}
