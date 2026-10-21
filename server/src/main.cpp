#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <optional>
#include <system_error>
#include <thread>

#include <asio.hpp>

#include "Options.hpp"
#include "Protocol.hpp"

namespace {

using Clock = std::chrono::steady_clock;
using Udp = asio::ip::udp;
using namespace std::chrono_literals;
namespace protocol = ntype::protocol;
volatile std::sig_atomic_t stopped = 0;

void on_signal(int) { stopped = 1; }

struct Player {
    Udp::endpoint endpoint;
    std::uint32_t nonce = 0;
    std::optional<std::uint32_t> last_sequence;
    std::uint8_t input = 0;
    float x = 0.0F;
    float y = 280.0F;
    Clock::time_point last_seen = Clock::now();
    Clock::time_point last_input = Clock::now();
};

class Server {
public:
    explicit Server(const std::uint16_t port) : socket_(io_, Udp::endpoint(Udp::v4(), port)) {
        socket_.non_blocking(true);
    }

    void run() {
        std::cout << "UDP server on port " << socket_.local_endpoint().port()
                  << " (2 players). Ctrl+C to stop." << std::endl;
        auto next = Clock::now();
        while (!stopped) {
            const auto now = Clock::now();
            for (auto& player : players_) {
                if (player && now - player->last_seen > 3s) {
                    player.reset();
                }
            }
            receive(now);
            move(now);
            if (++tick_ % 2 == 0) {
                broadcast();
            }
            next += std::chrono::nanoseconds(1'000'000'000 / 60);
            if (Clock::now() > next + 100ms) {
                next = Clock::now();
            }
            std::this_thread::sleep_until(next);
        }
    }

private:
    void send(const Udp::endpoint& to, const protocol::Message& message) {
        const auto bytes = protocol::encode(message);
        std::error_code error;
        socket_.send_to(asio::buffer(bytes), to, 0, error);
        if (error && error != asio::error::would_block && error != asio::error::try_again) {
            std::cerr << "Send error: " << error.message() << '\n';
        }
    }

    void receive(const Clock::time_point now) {
        for (int count = 0; count < 256; ++count) {
            std::array<std::uint8_t, 256> bytes{};
            Udp::endpoint from;
            std::error_code error;
            const auto size = socket_.receive_from(asio::buffer(bytes), from, 0, error);
            if (error == asio::error::would_block || error == asio::error::try_again) {
                break;
            }
            if (error) {
                std::cerr << "Receive error: " << error.message() << '\n';
                continue;
            }
            const auto message = protocol::decode({bytes.data(), size});
            if (message) {
                handle(from, *message, now);
            }
        }
    }

    void handle(const Udp::endpoint& from, const protocol::Message& message,
                const Clock::time_point now) {
        auto found = std::find_if(players_.begin(), players_.end(), [&](const auto& player) {
            return player && player->endpoint == from;
        });
        if (message.type == protocol::Type::Connect) {
            if (found == players_.end()) {
                found = std::find_if(players_.begin(), players_.end(),
                                     [](const auto& player) { return !player; });
                if (found == players_.end()) {
                    protocol::Message full;
                    full.type = protocol::Type::Full;
                    full.nonce = message.nonce;
                    send(from, full);
                    return;
                }
            }
            if (!*found || (*found)->nonce != message.nonce) {
                const auto slot = static_cast<std::size_t>(found - players_.begin());
                Player player;
                player.endpoint = from;
                player.nonce = message.nonce;
                player.x = slot == 0 ? 200.0F : 560.0F;
                *found = player;
                std::cout << "Player " << slot + 1 << " connected" << std::endl;
            }
            (*found)->last_seen = now;
            // The next periodic full state also acknowledges the connection.
            return;
        }
        if (found == players_.end() || (*found)->nonce != message.nonce) {
            return;
        }
        auto& player = **found;
        if (message.type == protocol::Type::Disconnect) {
            found->reset();
        } else if (message.type == protocol::Type::Input &&
                   (!player.last_sequence ||
                    protocol::newer(message.sequence, *player.last_sequence))) {
            player.last_sequence = message.sequence;
            player.input = message.bits;
            player.last_seen = now;
            player.last_input = now;
        }
    }

    void move(const Clock::time_point now) {
        for (auto& player : players_) {
            if (!player) {
                continue;
            }
            const auto input = now - player->last_input < 250ms ? player->input : 0;
            float x = static_cast<float>((input & protocol::kRight) != 0) -
                      static_cast<float>((input & protocol::kLeft) != 0);
            float y = static_cast<float>((input & protocol::kDown) != 0) -
                      static_cast<float>((input & protocol::kUp) != 0);
            const float length = std::hypot(x, y);
            if (length > 0.0F) {
                x /= length;
                y /= length;
            }
            player->x = std::clamp(player->x + x * protocol::kSpeed / 60.0F,
                                    0.0F, static_cast<float>(protocol::kWidth) - protocol::kSquareSize);
            player->y = std::clamp(player->y + y * protocol::kSpeed / 60.0F,
                                    0.0F, static_cast<float>(protocol::kHeight) - protocol::kSquareSize);
        }
    }

    void broadcast() {
        protocol::Message message;
        message.type = protocol::Type::State;
        message.sequence = tick_;
        for (std::size_t slot = 0; slot < players_.size(); ++slot) {
            if (players_[slot]) {
                message.bits |= static_cast<std::uint8_t>(1U << slot);
                message.positions[slot] = {static_cast<std::uint16_t>(players_[slot]->x),
                                            static_cast<std::uint16_t>(players_[slot]->y)};
            }
        }
        for (std::size_t slot = 0; slot < players_.size(); ++slot) {
            if (players_[slot]) {
                message.slot = static_cast<std::uint8_t>(slot);
                message.nonce = players_[slot]->nonce;
                send(players_[slot]->endpoint, message);
            }
        }
    }

    asio::io_context io_;
    Udp::socket socket_;
    std::array<std::optional<Player>, 2> players_;
    std::uint32_t tick_ = 0;
};

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = ntype::parse_options(argc, argv, ntype::Application::Server);
        if (options.help) {
            std::cout << "Usage: r-type_server [-p port]\n";
            return 0;
        }
        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);
        Server server(options.port);
        server.run();
    } catch (const std::exception& error) {
        std::cerr << "Server error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
