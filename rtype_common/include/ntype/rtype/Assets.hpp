#pragma once

#include <filesystem>
#include <optional>

namespace ntype::rtype {

/// Locates the assets/ directory (the one containing data/game.json).
///
/// With an explicit directory, only that one is accepted. Otherwise the
/// working directory and the executable's directory and its parents are
/// searched, so the binaries work both from the build tree and installed.
[[nodiscard]] std::optional<std::filesystem::path>
find_assets_directory(const std::filesystem::path& explicit_directory, const char* argv0);

} // namespace ntype::rtype
