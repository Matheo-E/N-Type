#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace ntype::engine::events {

namespace detail {

struct HandlerSlot {
    std::uint64_t id = 0;
    std::function<void(const void*)> handler;
};

/// Shared between the bus and its subscriptions, so a subscription that
/// outlives its bus does not dangle.
struct EventBusState {
    std::unordered_map<std::type_index, std::vector<HandlerSlot>> handlers;
    std::uint64_t next_id = 1;
    int dispatch_depth = 0;
    bool needs_compaction = false;

    void unsubscribe(std::type_index type, std::uint64_t id);
    void compact();
};

} // namespace detail

/// RAII token: the handler stays registered while the subscription lives.
class Subscription {
public:
    Subscription() = default;
    Subscription(std::weak_ptr<detail::EventBusState> state, std::type_index type,
                 std::uint64_t id) noexcept;
    ~Subscription();

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
    Subscription(Subscription&& other) noexcept;
    Subscription& operator=(Subscription&& other) noexcept;

    /// Unregisters the handler now instead of at destruction.
    void reset();
    [[nodiscard]] bool is_active() const noexcept { return id_ != 0 && !state_.expired(); }

private:
    std::weak_ptr<detail::EventBusState> state_;
    std::type_index type_{typeid(void)};
    std::uint64_t id_ = 0;
};

} // namespace ntype::engine::events
