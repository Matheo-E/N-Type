#include <iostream>

#include <ntype/engine/Version.hpp>
#include <ntype/rtype/GameInfo.hpp>

int main() {
    std::cout << ntype::rtype::game_name() << " server (engine " << ntype::engine::version()
              << ")\n";
    return 0;
}
