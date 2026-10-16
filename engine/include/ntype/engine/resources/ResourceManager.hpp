#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace ntype::engine::resources {

/// Caches resources of one type by string id. The loader turns a path into
/// a resource, or throws when the file cannot be loaded.
template <typename T>
class ResourceManager {
public:
    using Loader = std::function<std::unique_ptr<T>(const std::filesystem::path&)>;

    explicit ResourceManager(Loader loader) : loader_(std::move(loader)) {}

    /// Loads the resource once; later calls with the same id return the cache.
    T& load(const std::string& id, const std::filesystem::path& path) {
        if (const auto it = cache_.find(id); it != cache_.end()) {
            return *it->second;
        }
        auto resource = loader_(path);
        if (!resource) {
            throw std::runtime_error("failed to load resource '" + id + "' from " + path.string());
        }
        return *cache_.emplace(id, std::move(resource)).first->second;
    }

    /// Throws std::out_of_range for an unknown id.
    [[nodiscard]] T& get(const std::string& id) const {
        const auto it = cache_.find(id);
        if (it == cache_.end()) {
            throw std::out_of_range("unknown resource '" + id + "'");
        }
        return *it->second;
    }

    [[nodiscard]] T* find(const std::string& id) const noexcept {
        const auto it = cache_.find(id);
        return it == cache_.end() ? nullptr : it->second.get();
    }

    [[nodiscard]] bool contains(const std::string& id) const noexcept {
        return cache_.contains(id);
    }

    void unload(const std::string& id) { cache_.erase(id); }
    void clear() noexcept { cache_.clear(); }

private:
    Loader loader_;
    std::unordered_map<std::string, std::unique_ptr<T>> cache_;
};

} // namespace ntype::engine::resources
