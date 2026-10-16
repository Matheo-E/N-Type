#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace ntype::client {

struct ClientConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = 4242;
    std::filesystem::path assets;
    /// Developer option: save a frame after a few seconds of play, then quit.
    std::filesystem::path screenshot;
    bool show_help = false;

    [[nodiscard]] static std::expected<ClientConfig, std::string>
    from_args(std::span<const std::string_view> args);
    [[nodiscard]] static std::string_view usage() noexcept;
};

} // namespace ntype::client
