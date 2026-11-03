#pragma once

#include <mutex>
#include <utility>
#include <vector>

namespace ntype::server {

// Hands items from the network thread to the game thread.
template <typename T>
class ThreadSafeQueue {
public:
    void push(T item) {
        std::lock_guard lock(mutex_);
        items_.push_back(std::move(item));
    }

    // Takes everything queued so far in one lock.
    std::vector<T> drain() {
        std::lock_guard lock(mutex_);
        return std::exchange(items_, {});
    }

private:
    std::mutex mutex_;
    std::vector<T> items_;
};

} // namespace ntype::server
