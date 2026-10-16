#include <iostream>

#include <ntype/engine/Version.hpp>

int main() {
    std::cout << "R-Type client (engine " << ntype::engine::version() << ")\n";
    return 0;
}
