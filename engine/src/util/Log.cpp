#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>

#include <ntype/engine/util/Log.hpp>

namespace ntype::engine::log {

namespace {

std::atomic<Level> g_level{Level::Info};
std::mutex g_output_mutex;

std::string_view label(const Level level) noexcept {
    switch (level) {
    case Level::Debug:
        return "debug";
    case Level::Info:
        return "info";
    case Level::Warning:
        return "warn";
    case Level::Error:
        return "error";
    }
    return "?";
}

} // namespace

void set_level(const Level level) noexcept {
    g_level.store(level, std::memory_order_relaxed);
}

void write(const Level level, const std::string_view message) {
    if (level < g_level.load(std::memory_order_relaxed)) {
        return;
    }
    const auto now =
        std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
    const std::string line = std::format("[{:%T}] [{}] {}\n", now, label(level), message);
    const std::scoped_lock lock(g_output_mutex);
    auto& stream = level >= Level::Warning ? std::cerr : std::cout;
    stream << line << std::flush;
}

} // namespace ntype::engine::log
