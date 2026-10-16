#pragma once

#include <string>

namespace ntype::engine::sfml {

/// Version of the SFML library the backend was built against.
[[nodiscard]] std::string sfml_version();

} // namespace ntype::engine::sfml
