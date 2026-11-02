#include "client/src/ClientNetwork.hpp"
#include "server/src/Server.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <thread>

using namespace std::chrono_literals;
using ntype::client::ClientNetwork;
using ntype::client::Phase;
using protocol::LobbyPhase;

namespace {

// A real server on its own thread, like the r-type_server process.
struct RunningServer {
    explicit RunningServer(std::uint16_t port) : server(port), thread([this] { server.run(running); }) {}
    ~RunningServer() {
        running = false;
        thread.join();
    }
    std::atomic<bool> running{true};
    ntype::server::Server server;
    std::thread thread;
};

// Updates the clients like the game loop would, until `done` or the timeout.
bool pump(std::initializer_list<ClientNetwork*> clients, const std::function<bool()>& done,
          std::chrono::milliseconds timeout = 2000ms) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        for (auto* client : clients) {
            client->update(0);
        }
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

std::string lower(std::string text) {
    for (auto& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

} // namespace

int main() {
    auto running = std::make_unique<RunningServer>(0);
    const auto port = running->server.port();
    ClientNetwork a("127.0.0.1", port, "Ann");
    ClientNetwork b("127.0.0.1", port, "Bo");
    ClientNetwork c("127.0.0.1", port, "Cy");
    const auto all = {&a, &b, &c};
    assert(pump(all, [&] { return a.phase() == Phase::Browsing && b.phase() == Phase::Browsing &&
                                  c.phase() == Phase::Browsing; }));

    a.create("Alpha", 1);
    assert(pump(all, [&] { return a.phase() == Phase::InLobby; }));
    assert(a.is_leader() && a.lobby()->code.size() == 4 && !a.busy());
    const auto code = a.lobby()->code;
    assert(pump(all, [&] { return !b.lobbies().empty() && b.lobbies()[0].name == "Alpha"; }));

    b.join(lower(code));
    assert(pump(all, [&] { return b.phase() == Phase::InLobby && b.lobby()->members.size() == 2; }));
    assert(!b.is_leader());

    // Not the leader: ignored, and the request gives up after 3 s.
    b.start();
    assert(!pump(all, [&] { return b.lobby()->state == LobbyPhase::Playing; }, 500ms));
    assert(pump(all, [&] { return !b.busy(); }, 4000ms));
    assert(b.error() == "Server did not respond");
    b.clear_error();

    a.rename("Beta");
    assert(pump(all, [&] { return !a.busy(); }));
    a.set_map(2);
    assert(pump(all, [&] { return b.lobby()->name == "Beta" && b.lobby()->map == 2; }));

    a.start();
    assert(pump(all, [&] { return a.players().size() == 2 && b.players().size() == 2; }));
    assert(b.lobby()->state == LobbyPhase::Playing);

    c.join(code);
    assert(pump(all, [&] { return c.phase() == Phase::InLobby; }));
    assert(c.is_spectator());

    // Leaving is immediate on our side and stays so: late states do not pull us back.
    a.leave();
    assert(a.phase() == Phase::Browsing && !a.lobby() && a.players().empty());
    assert(!pump(all, [&] { return a.phase() != Phase::Browsing; }, 500ms));
    assert(pump(all, [&] { return b.is_leader(); }));

    c.leave();
    c.join("ZZZZ");
    assert(pump(all, [&] { return !c.error().empty(); }));
    assert(c.error() == "No lobby with this code" && c.phase() == Phase::Browsing);

    // The server goes away: back to connecting, then browsing on a new server.
    running.reset();
    assert(pump({&b}, [&] { return b.phase() == Phase::Connecting; }, 7000ms));
    assert(b.status() == "Connection lost - reconnecting" && !b.lobby());
    running = std::make_unique<RunningServer>(port);
    assert(pump({&b}, [&] { return b.phase() == Phase::Browsing; }, 3000ms));
    assert(!b.lobby());

    std::cout << "ClientNetwork tests passed!\n";
}
