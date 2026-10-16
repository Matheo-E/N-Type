#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ntype::server {

struct ServerConfig {
    std::uint16_t port = 4242;
    unsigned tick_rate = 60;
    /// Snapshots are sent every `snapshot_interval` ticks.
    unsigned snapshot_interval = 2;
    unsigned max_rooms = 4;
    std::chrono::milliseconds client_timeout{5000};
    std::optional<std::uint32_t> seed;
    std::filesystem::path assets;
    bool show_help = false;

    /// Parses the command line (without argv[0]). Returns an error message
    /// for unknown options or invalid values.
    [[nodiscard]] static std::expected<ServerConfig, std::string>
    from_args(std::span<const std::string_view> args);

    [[nodiscard]] static std::string_view usage() noexcept;
};

} // namespace ntype::server
