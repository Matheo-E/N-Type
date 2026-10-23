#pragma once

#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include "GameConfig.hpp"

namespace ntype {

struct Options {
    std::string host = "127.0.0.1";
    std::uint16_t port = config::kDefaultPort;
    bool help = false;
};

enum class Application { Client, Server };

inline Options parse_options(int argc, char** argv, Application application) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view arg(argv[index]);
        if (arg == "--help") {
            options.help = true;
        } else if (arg == "-h" && application == Application::Client && index + 1 < argc) {
            options.host = argv[++index];
        } else if (arg == "-p" && index + 1 < argc) {
            const std::string_view text(argv[++index]);
            unsigned value = 0;
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
                value == 0 || value > 65535) {
                throw std::invalid_argument("Port must be between 1 and 65535");
            }
            options.port = static_cast<std::uint16_t>(value);
        } else {
            throw std::invalid_argument("Unknown or incomplete option: " + std::string(arg));
        }
    }
    return options;
}

} // namespace ntype
