#pragma once

#include <string_view>

namespace ntype::engine {

/// Human-readable engine version, e.g. "0.1.0".
[[nodiscard]] std::string_view version() noexcept;

} // namespace ntype::engine
