cmake_minimum_required(VERSION 3.25)
project(NTypeSquares VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(asio CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Binary protocol shared by the client and the server (docs/PROTOCOL_v2.md).
add_library(ntype_protocol STATIC common/protocol/Protocol.cpp)
target_include_directories(ntype_protocol PUBLIC "${CMAKE_SOURCE_DIR}/common/protocol")

# Common data format and network dependency.
add_library(ntype_shared INTERFACE)
target_include_directories(ntype_shared INTERFACE "${CMAKE_SOURCE_DIR}/shared")
target_link_libraries(ntype_shared INTERFACE ntype_protocol asio::asio Threads::Threads)
target_compile_definitions(ntype_shared INTERFACE ASIO_NO_DEPRECATED)
if(WIN32)
    target_compile_definitions(ntype_shared INTERFACE _WIN32_WINNT=0x0A00)
    target_link_libraries(ntype_shared INTERFACE ws2_32 mswsock)
endif()

# ECS engine (engine/, owned by the engine branch): both executables run on it.
# It is written against SFML 2.6, so the server needs SFML too.
find_package(SFML 2.6 CONFIG REQUIRED COMPONENTS graphics window system)
add_library(ntype_engine STATIC engine/src/Factory.cpp engine/src/System.cpp)
target_include_directories(ntype_engine PUBLIC "${CMAKE_SOURCE_DIR}/engine/include")
target_link_libraries(ntype_engine PUBLIC sfml-graphics sfml-window sfml-system)

option(NTYPE_BUILD_CLIENT "Build the graphical client" ON)
if(NTYPE_BUILD_CLIENT)
    add_subdirectory(client)
endif()
add_subdirectory(server)

enable_testing()
add_executable(protocol_test tests/protocol/protocol_test.cpp)
target_include_directories(protocol_test PRIVATE "${CMAKE_SOURCE_DIR}")
target_link_libraries(protocol_test PRIVATE ntype_protocol)
# The test is made of assert(): keep them in Release builds too.
target_compile_options(protocol_test PRIVATE -UNDEBUG)
add_test(NAME protocol COMMAND protocol_test)
