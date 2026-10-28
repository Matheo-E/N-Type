#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ntype::client {

struct ConnectionSettings {
    std::string name;
    std::string host;
    std::uint16_t port;
};

std::optional<ConnectionSettings> show_launcher(const ConnectionSettings& defaults);

} // namespace ntype::client
