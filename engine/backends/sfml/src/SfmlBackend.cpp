#include <format>

#include <SFML/Config.hpp>

#include <ntype/engine/sfml/SfmlBackend.hpp>

namespace ntype::engine::sfml {

std::string sfml_version() {
    return std::format("{}.{}.{}", SFML_VERSION_MAJOR, SFML_VERSION_MINOR, SFML_VERSION_PATCH);
}

} // namespace ntype::engine::sfml
