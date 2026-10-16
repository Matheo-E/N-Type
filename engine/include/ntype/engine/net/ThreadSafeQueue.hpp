#pragma once

#include <deque>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace ntype::engine::net {

/// Mutex-protected FIFO: the only state shared between the network thread
/// and the game thread.
template <typename T>
class ThreadSafeQueue {
public:
    void push(T item) {
        const std::scoped_lock lock(mutex_);
        items_.push_back(std::move(item));
    }

    [[nodiscard]] std::optional<T> try_pop() {
        const std::scoped_lock lock(mutex_);
        if (items_.empty()) {
            return std::nullopt;
        }
        T item = std::move(items_.front());
        items_.pop_front();
        return item;
    }

    /// Takes every queued item at once (one lock per batch).
    [[nodiscard]] std::vector<T> drain() {
        std::deque<T> taken;
        {
            const std::scoped_lock lock(mutex_);
            taken.swap(items_);
        }
        return {std::make_move_iterator(taken.begin()), std::make_move_iterator(taken.end())};
    }

    [[nodiscard]] std::size_t size() const {
        const std::scoped_lock lock(mutex_);
        return items_.size();
    }

private:
    mutable std::mutex mutex_;
    std::deque<T> items_;
};

} // namespace ntype::engine::net
