#include "Console.hpp"

#include <iostream>
#include <thread>

namespace ntype::server {

Console::Console() : lines_(std::make_shared<ThreadSafeQueue<std::string>>()) {
    std::thread([lines = lines_] {
        std::string line;
        while (std::getline(std::cin, line)) {
            lines->push(line);
        }
    }).detach();
}

} // namespace ntype::server
