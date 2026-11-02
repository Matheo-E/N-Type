#include "Theme.hpp"

#include <stdexcept>

namespace ntype::client::ui {

Theme load_theme() {
    const std::array<const char*, 9> paths{
#ifdef _WIN32
        "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
#endif
        // Fedora
        "/usr/share/fonts/google-noto/NotoSans-Regular.ttf", "/usr/share/fonts/adwaita-sans-fonts/AdwaitaSans-Regular.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf", "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
        "/usr/local/share/fonts/DejaVuSans.ttf", "DejaVuSans.ttf", "Arial.ttf"};
    Theme theme;
    for (const auto* path : paths) {
        if (theme.font.loadFromFile(path)) {
            return theme;
        }
    }
    throw std::runtime_error("No usable font found");
}

} // namespace ntype::client::ui
