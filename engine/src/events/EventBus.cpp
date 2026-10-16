#include <algorithm>
#include <stdexcept>

#include <ntype/engine/events/EventBus.hpp>

namespace ntype::engine::events {

namespace detail {

void EventBusState::unsubscribe(const std::type_index type, const std::uint64_t id) {
    const auto it = handlers.find(type);
    if (it == handlers.end()) {
        return;
    }
    for (auto& slot : it->second) {
        if (slot.id == id) {
            slot.handler = nullptr;
        }
    }
    needs_compaction = true;
    if (dispatch_depth == 0) {
        compact();
    }
}

void EventBusState::compact() {
    for (auto& [type, slots] : handlers) {
        std::erase_if(slots, [](const HandlerSlot& slot) { return !slot.handler; });
    }
    needs_compaction = false;
}

} // namespace detail

Subscription::Subscription(std::weak_ptr<detail::EventBusState> state, const std::type_index type,
                           const std::uint64_t id) noexcept
    : state_(std::move(state)), type_(type), id_(id) {}

Subscription::~Subscription() {
    reset();
}

Subscription::Subscription(Subscription&& other) noexcept
    : state_(std::move(other.state_)), type_(other.type_), id_(std::exchange(other.id_, 0)) {}

Subscription& Subscription::operator=(Subscription&& other) noexcept {
    if (this != &other) {
        reset();
        state_ = std::move(other.state_);
        type_ = other.type_;
        id_ = std::exchange(other.id_, 0);
    }
    return *this;
}

void Subscription::reset() {
    if (id_ == 0) {
        return;
    }
    if (const auto state = state_.lock()) {
        state->unsubscribe(type_, id_);
    }
    id_ = 0;
    state_.reset();
}

void EventBus::dispatch_queued() {
    // Bounded so a handler that always re-enqueues cannot hang the tick.
    constexpr int kMaxRounds = 64;
    for (int round = 0; round < kMaxRounds && !queued_.empty(); ++round) {
        auto batch = std::exchange(queued_, {});
        for (const auto& deliver : batch) {
            deliver();
        }
    }
    if (!queued_.empty()) {
        queued_.clear();
        throw std::runtime_error("EventBus: events keep re-enqueueing themselves");
    }
}

} // namespace ntype::engine::events
