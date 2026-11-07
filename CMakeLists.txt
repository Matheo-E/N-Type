cmake_minimum_required(VERSION 3.25)
project(ntype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)
find_package(nlohmann_json 3 REQUIRED)

add_executable(ntype
  src/main.cpp
  engine/src/System.cpp
  engine/src/Factory.cpp
  engine/src/Loader.cpp)
target_link_libraries(ntype PRIVATE sfml-graphics sfml-window sfml-system nlohmann_json::nlohmann_json)
target_compile_options(ntype PRIVATE -Wall -Wextra -Wpedantic)

# Absolute path to assets/, so the game runs from any working directory.
target_compile_definitions(ntype PRIVATE NTYPE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
