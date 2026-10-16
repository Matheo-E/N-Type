#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <typeindex>
#include <utility>
#include <vector>

#include <ntype/engine/events/Subscription.hpp>

namespace ntype::engine::events {

/// Typed publish/subscribe between systems (Mediator).
///
/// `publish` delivers immediately. `enqueue` stores the event until
/// `dispatch_queued`, which gives a deterministic delivery point per tick.
/// Handlers may subscribe, unsubscribe or enqueue while being called.
class EventBus {
public:
    EventBus() : state_(std::make_shared<detail::EventBusState>()) {}

    template <typename E>
    [[nodiscard]] Subscription subscribe(std::function<void(const E&)> handler) {
        const std::uint64_t id = state_->next_id++;
        const std::type_index type(typeid(E));
        state_->handlers[type].push_back({id, [handler = std::move(handler)](const void* event) {
                                              handler(*static_cast<const E*>(event));
                                          }});
        return Subscription{state_, type, id};
    }

    template <typename E>
    void publish(const E& event) {
        const auto it = state_->handlers.find(std::type_index(typeid(E)));
        if (it == state_->handlers.end()) {
            return;
        }
        ++state_->dispatch_depth;
        // Index-based: handlers added during the loop are not called this time,
        // and removed ones are only blanked until compaction.
        // The handler is copied because a subscribe() inside it may reallocate
        // the vector while it runs.
        const std::type_index type(typeid(E));
        const std::size_t count = it->second.size();
        for (std::size_t i = 0; i < count; ++i) {
            const auto handler = state_->handlers[type][i].handler;
            if (handler) {
                handler(&event);
            }
        }
        --state_->dispatch_depth;
        if (state_->dispatch_depth == 0 && state_->needs_compaction) {
            state_->compact();
        }
    }

    template <typename E>
    void enqueue(E event) {
        queued_.push_back([this, event = std::move(event)] { publish(event); });
    }

    /// Delivers queued events, including those enqueued by handlers meanwhile.
    void dispatch_queued();
    void clear_queue() noexcept { queued_.clear(); }
    [[nodiscard]] std::size_t queued_count() const noexcept { return queued_.size(); }

private:
    std::shared_ptr<detail::EventBusState> state_;
    std::vector<std::function<void()>> queued_;
};

} // namespace ntype::engine::events
