#pragma once

#include <format>
#include <string_view>
#include <utility>

namespace ntype::engine::log {

enum class Level { Debug, Info, Warning, Error };

/// Minimum level that gets printed. Defaults to Info.
void set_level(Level level) noexcept;

/// Thread-safe: one call prints one whole line.
void write(Level level, std::string_view message);

template <typename... Args>
void debug(std::format_string<Args...> format, Args&&... args) {
    write(Level::Debug, std::format(format, std::forward<Args>(args)...));
}
template <typename... Args>
void info(std::format_string<Args...> format, Args&&... args) {
    write(Level::Info, std::format(format, std::forward<Args>(args)...));
}
template <typename... Args>
void warn(std::format_string<Args...> format, Args&&... args) {
    write(Level::Warning, std::format(format, std::forward<Args>(args)...));
}
template <typename... Args>
void error(std::format_string<Args...> format, Args&&... args) {
    write(Level::Error, std::format(format, std::forward<Args>(args)...));
}

} // namespace ntype::engine::log
