#include "client/src/ClientNetwork.hpp"
#include "server/src/Server.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;
using ntype::client::ClientNetwork;
using ntype::client::Phase;
using protocol::LobbyPhase;

namespace {

// A real server on its own thread, like the r-type_server process.
struct RunningServer {
    // No enemies: these checks are about the session, not gameplay.
    explicit RunningServer(std::uint16_t port) : server(port, true), thread([this] { server.run(running); }) {}
    ~RunningServer() {
        running = false;
        thread.join();
    }
    std::atomic<bool> running{true};
    ntype::server::Server server;
    std::thread thread;
};

// Forwards UDP between one client and the server, dropping what the test asks for.
class Relay {
public:
    explicit Relay(std::uint16_t server_port)
        : socket_(io_, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0)),
          server_(asio::ip::make_address("127.0.0.1"), server_port), port_(socket_.local_endpoint().port()) {
        socket_.non_blocking(true);
        thread_ = std::thread([this] { run(); });
    }
    ~Relay() {
        stop_ = true;
        thread_.join();
    }
    std::uint16_t port() const { return port_; }

    std::atomic<bool> drop_next_leave{false};
    std::atomic<bool> drop_states{false};

private:
    void run() {
        std::array<std::uint8_t, 1500> bytes{};
        asio::ip::udp::endpoint from;
        while (!stop_) {
            std::error_code error;
            const std::size_t size = socket_.receive_from(asio::buffer(bytes), from, 0, error);
            if (error || size == 0) {
                std::this_thread::sleep_for(1ms);
                continue;
            }
            const auto type = static_cast<protocol::MessageType>(bytes[0]);
            if (from == server_) {
                if (!(drop_states && type == protocol::MessageType::LobbyState)) {
                    socket_.send_to(asio::buffer(bytes.data(), size), client_, 0, error);
                }
            } else {
                client_ = from;
                if (!(type == protocol::MessageType::LobbyLeave && drop_next_leave.exchange(false))) {
                    socket_.send_to(asio::buffer(bytes.data(), size), server_, 0, error);
                }
            }
        }
    }

    asio::io_context io_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint server_;
    asio::ip::udp::endpoint client_;
    std::uint16_t port_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

// Updates the clients like the game loop would, until `done` or the timeout.
bool pump(std::initializer_list<ClientNetwork*> clients, const std::function<bool()>& done,
          std::chrono::milliseconds timeout = 2000ms) {
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + timeout;
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

// A hand-driven server: the test decides every packet the client receives.
class FakeServer {
public:
    FakeServer() : socket_(io_, asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0)) {
        socket_.non_blocking(true);
    }
    std::uint16_t port() const { return socket_.local_endpoint().port(); }

    // Reads what the client sent; remembers where to answer. Returns the message types.
    std::vector<protocol::MessageType> receive() {
        std::vector<protocol::MessageType> types;
        std::array<std::uint8_t, 1500> bytes{};
        std::error_code error;
        for (;;) {
            const std::size_t size = socket_.receive_from(asio::buffer(bytes), client_, 0, error);
            if (error || size == 0) {
                return types;
            }
            types.push_back(static_cast<protocol::MessageType>(bytes[0]));
        }
    }
    void send(const std::vector<std::uint8_t>& bytes) {
        std::error_code error;
        socket_.send_to(asio::buffer(bytes), client_, 0, error);
    }

private:
    asio::io_context io_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint client_;
};

// A frame shows once all its parts are there, in any order; older parts are dropped.
void snapshot_parts() {
    FakeServer server;
    ClientNetwork client("127.0.0.1", server.port(), "Me");
    const auto settle = [&] {
        for (int i = 0; i < 5; ++i) {
            client.update(0);
            std::this_thread::sleep_for(5ms);
        }
    };
    settle();
    assert(!server.receive().empty()); // the connect request
    server.send(protocol::makeConnectAccept());
    settle();
    assert(client.phase() == Phase::Browsing);
    client.create("Fake");
    protocol::LobbyState state{"K7QX", "Fake", protocol::LobbyPhase::Playing, 1, 1,
                               {{1, protocol::MemberRole::Player, "Me"}}};
    server.receive();
    server.send(protocol::makeLobbyState(state));
    settle();
    assert(client.phase() == Phase::InLobby);

    const protocol::Entity ship{1, 1, 10, 20, 0, 100};
    const protocol::Entity enemy{40, 2, 600, 300, 3, 50};
    server.send(protocol::makeWorldSnapshot(5, 1, 2, {enemy}));
    settle();
    assert(client.entities().empty()); // half a frame: nothing yet
    server.send(protocol::makeWorldSnapshot(5, 0, 2, {ship}));
    settle();
    assert(client.entities().size() == 2 && client.players().size() == 1 && client.players()[0].id == 1);

    server.send(protocol::makeWorldSnapshot(4, 0, 1, {})); // older than what is shown
    settle();
    assert(client.entities().size() == 2);

    server.send(protocol::makeWorldSnapshot(7, 0, 2, {ship})); // never completed...
    server.send(protocol::makeWorldSnapshot(8, 0, 1, {enemy})); // ...a newer frame wins
    server.send(protocol::makeWorldSnapshot(7, 1, 2, {enemy})); // late part: dropped
    settle();
    assert(client.entities().size() == 1 && client.entities()[0].id == 40 && client.players().empty());

    server.send(protocol::makeEntityDied(enemy));
    settle();
    const std::vector<protocol::Entity> deaths = client.take_deaths();
    assert(deaths.size() == 1 && deaths[0].id == 40 && deaths[0].type == 3 && client.take_deaths().empty());
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
    const std::uint16_t port = running->server.port();

    // Names the protocol cannot carry are refused up front, not thrown later from update().
    for (const char* name : {"Math\xC3\xA9o", "Seventeen_letters", ""}) {
        bool refused = false;
        try {
            ClientNetwork bad("127.0.0.1", port, name);
        } catch (const std::invalid_argument&) {
            refused = true;
        }
        assert(refused);
    }
    ClientNetwork a("127.0.0.1", port, "Ann");
    ClientNetwork b("127.0.0.1", port, "Bo");
    ClientNetwork c("127.0.0.1", port, "Cy");
    const std::initializer_list<ClientNetwork*> all = {&a, &b, &c};
    assert(pump(all, [&] { return a.phase() == Phase::Browsing && b.phase() == Phase::Browsing &&
                                  c.phase() == Phase::Browsing; }));

    a.create("Alpha");
    assert(pump(all, [&] { return a.phase() == Phase::InLobby; }));
    assert(a.is_leader() && a.lobby()->code.size() == 4 && !a.busy());
    const std::string code = a.lobby()->code;
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
    assert(pump(all, [&] { return b.lobby()->name == "Beta"; }));

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

    // A lost LobbyLeave is re-sent until the server confirms it, even when no
    // late lobby state comes back to trigger the self-healing leave.
    {
        Relay relay(port);
        ClientNetwork d("127.0.0.1", relay.port(), "Di");
        const std::initializer_list<ClientNetwork*> clients = {&a, &b, &c, &d};
        assert(pump(clients, [&] { return d.phase() == Phase::Browsing; }));
        d.create("Delta");
        assert(pump(clients, [&] { return d.phase() == Phase::InLobby; }));
        const std::string delta = d.lobby()->code;
        const auto listed = [&] {
            return std::any_of(c.lobbies().begin(), c.lobbies().end(),
                               [&](const protocol::LobbyInfo& lobby) { return lobby.code == delta; });
        };
        assert(pump(clients, listed));
        // Stay longer than the list refresh period, as a player would.
        pump(clients, [] { return false; }, 1200ms);
        relay.drop_next_leave = true;
        relay.drop_states = true;
        d.leave();
        assert(pump(clients, [&] { return !d.busy() && !listed(); }, 3000ms));
    }

    // The server goes away: back to connecting, then browsing on a new server.
    running.reset();
    assert(pump({&b}, [&] { return b.phase() == Phase::Connecting; }, 7000ms));
    assert(b.status() == "Connection lost - reconnecting" && !b.lobby());
    running = std::make_unique<RunningServer>(port);
    assert(pump({&b}, [&] { return b.phase() == Phase::Browsing; }, 3000ms));
    assert(!b.lobby());

    snapshot_parts();
    std::cout << "ClientNetwork tests passed!\n";
}
