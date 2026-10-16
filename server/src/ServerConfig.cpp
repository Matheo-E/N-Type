#include "ServerConfig.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <limits>

namespace ntype::server {

namespace {

template <typename T>
std::optional<T> parse_number(const std::string_view text, const T min, const T max) {
    T value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value < min || value > max) {
        return std::nullopt;
    }
    return value;
}

} // namespace

std::string_view ServerConfig::usage() noexcept {
    return "USAGE: r-type_server [options]\n"
           "  -p, --port <port>        UDP port to listen on (default 4242)\n"
           "  -t, --tick-rate <hz>     simulation rate (default 60)\n"
           "      --max-rooms <n>      maximum simultaneous games (default 4)\n"
           "      --timeout <ms>       client timeout in milliseconds (default 5000)\n"
           "      --seed <n>           random seed (default: random)\n"
           "      --assets <dir>       assets directory (default: auto-detected)\n"
           "  -h, --help               show this help\n";
}

std::expected<ServerConfig, std::string>
ServerConfig::from_args(const std::span<const std::string_view> args) {
    ServerConfig config;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view option = args[i];
        if (option == "-h" || option == "--help") {
            config.show_help = true;
            continue;
        }
        if (i + 1 >= args.size()) {
            return std::unexpected(std::format("missing value or unknown option '{}'", option));
        }
        const std::string_view value = args[++i];
        const auto invalid = [&] {
            return std::unexpected(std::format("invalid value '{}' for {}", value, option));
        };
        if (option == "-p" || option == "--port") {
            const auto port = parse_number<std::uint16_t>(value, 1, 65535);
            if (!port) {
                return invalid();
            }
            config.port = *port;
        } else if (option == "-t" || option == "--tick-rate") {
            const auto rate = parse_number<unsigned>(value, 10, 240);
            if (!rate) {
                return invalid();
            }
            config.tick_rate = *rate;
            config.snapshot_interval = std::max(1U, *rate / 30U);
        } else if (option == "--max-rooms") {
            const auto rooms = parse_number<unsigned>(value, 1, 64);
            if (!rooms) {
                return invalid();
            }
            config.max_rooms = *rooms;
        } else if (option == "--timeout") {
            const auto timeout = parse_number<unsigned>(value, 500, 600000);
            if (!timeout) {
                return invalid();
            }
            config.client_timeout = std::chrono::milliseconds(*timeout);
        } else if (option == "--seed") {
            const auto seed =
                parse_number<std::uint32_t>(value, 0, std::numeric_limits<std::uint32_t>::max());
            if (!seed) {
                return invalid();
            }
            config.seed = *seed;
        } else if (option == "--assets") {
            config.assets = std::filesystem::path(value);
        } else {
            return std::unexpected(std::format("unknown option '{}'", option));
        }
    }
    return config;
}

} // namespace ntype::server
