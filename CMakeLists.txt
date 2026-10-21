cmake_minimum_required(VERSION 3.25)
project(NTypeSquares VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(asio CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Common data format and network dependency; no graphics required by the server.
add_library(ntype_shared INTERFACE)
target_include_directories(ntype_shared INTERFACE "${CMAKE_SOURCE_DIR}/shared")
target_link_libraries(ntype_shared INTERFACE asio::asio Threads::Threads)
target_compile_definitions(ntype_shared INTERFACE ASIO_NO_DEPRECATED)
if(WIN32)
    target_compile_definitions(ntype_shared INTERFACE _WIN32_WINNT=0x0A00)
    target_link_libraries(ntype_shared INTERFACE ws2_32 mswsock)
endif()

option(NTYPE_BUILD_CLIENT "Build the graphical client" ON)
if(NTYPE_BUILD_CLIENT)
    find_package(SFML 3 CONFIG REQUIRED COMPONENTS Graphics Window System)
    add_subdirectory(client)
endif()
add_subdirectory(server)
