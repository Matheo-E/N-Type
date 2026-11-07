#pragma once

#include <algorithm>
#include <array>
#include <filesystem>

#include <SFML/Graphics/Font.hpp>

namespace ntype::client {

/**
 * @brief Loads the first available system UI font.
 *
 * Font locations differ between distributions, so the common Debian/Ubuntu,
 * Fedora and Arch paths are tried in turn before bare file names. Missing
 * files are skipped so SFML does not log an error for each absent candidate.
 */
inline bool load_ui_font(sf::Font& font) {
    static constexpr std::array paths{
#ifdef _WIN32
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
#else
        // Debian / Ubuntu
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        // Fedora
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf",
        "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
        "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
        // Arch
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        "/usr/local/share/fonts/DejaVuSans.ttf",
#endif
        "DejaVuSans.ttf",
        "Arial.ttf",
    };
    return std::any_of(paths.begin(), paths.end(),
                       [&](const char* path) {
                           std::error_code error;
                           return std::filesystem::exists(path, error) &&
                                  font.loadFromFile(path);
                       });
}

} // namespace ntype::client
