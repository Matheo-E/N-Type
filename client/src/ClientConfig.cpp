#include "ClientConfig.hpp"

#include <charconv>
#include <format>

namespace ntype::client {

std::string_view ClientConfig::usage() noexcept {
    return "USAGE: r-type_client [options]\n"
           "  -h, --host <host>   server address (default 127.0.0.1)\n"
           "  -p, --port <port>   server UDP port (default 4242)\n"
           "      --assets <dir>  assets directory (default: auto-detected)\n"
           "      --screenshot <file>  save a frame after 6 seconds, then quit\n"
           "      --help          show this help\n";
}

std::expected<ClientConfig, std::string>
ClientConfig::from_args(const std::span<const std::string_view> args) {
    ClientConfig config;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view option = args[i];
        if (option == "--help") {
            config.show_help = true;
            continue;
        }
        if (i + 1 >= args.size()) {
            return std::unexpected(std::format("missing value or unknown option '{}'", option));
        }
        const std::string_view value = args[++i];
        if (option == "-h" || option == "--host") {
            config.host = std::string(value);
        } else if (option == "-p" || option == "--port") {
            unsigned port = 0;
            const auto [end, error] =
                std::from_chars(value.data(), value.data() + value.size(), port);
            if (error != std::errc{} || end != value.data() + value.size() || port == 0 ||
                port > 65535) {
                return std::unexpected(std::format("invalid port '{}'", value));
            }
            config.port = static_cast<std::uint16_t>(port);
        } else if (option == "--assets") {
            config.assets = std::filesystem::path(value);
        } else if (option == "--screenshot") {
            config.screenshot = std::filesystem::path(value);
        } else {
            return std::unexpected(std::format("unknown option '{}'", option));
        }
    }
    return config;
}

} // namespace ntype::client
