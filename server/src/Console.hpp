#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ThreadSafeQueue.hpp"

namespace ntype::server {

// Reads commands typed on the server's standard input, one per line, on a
// detached thread (it may stay blocked in getline until the process ends).
// The game thread takes them with poll().
class Console {
public:
    Console();
    std::vector<std::string> poll() { return lines_->drain(); }

private:
    // Shared with the reading thread, which may outlive this object.
    std::shared_ptr<ThreadSafeQueue<std::string>> lines_;
};

} // namespace ntype::server
