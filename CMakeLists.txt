cmake_minimum_required(VERSION 3.25)
project(ntype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)

add_executable(ntype src/main.cpp engine/src/System.cpp engine/src/Factory.cpp)
target_link_libraries(ntype PRIVATE sfml-graphics sfml-window sfml-system)
