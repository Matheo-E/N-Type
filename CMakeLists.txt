cmake_minimum_required(VERSION 3.25)
project(NTypeSquares VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(asio CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json 3 CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(SFML 2.6 CONFIG REQUIRED COMPONENTS graphics window system audio)

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

# Shared ECS engine used by both network executables.
add_library(ntype_engine STATIC
    engine/src/Behaviour.cpp
    engine/src/Combat.cpp
    engine/src/Factory.cpp
    engine/src/Input.cpp
    engine/src/Items.cpp
    engine/src/Loader.cpp
    engine/src/System.cpp
)
target_include_directories(ntype_engine PUBLIC "${CMAKE_SOURCE_DIR}/engine/include")
target_link_libraries(ntype_engine
    PUBLIC sfml-graphics sfml-window sfml-system
    PRIVATE nlohmann_json::nlohmann_json
)

option(NTYPE_BUILD_CLIENT "Build the graphical client" ON)
if(NTYPE_BUILD_CLIENT)
    add_subdirectory(client)
endif()
add_subdirectory(server)

# Engine sandbox: the engine alone in one window, no network (sandbox/main.cpp).
if(NTYPE_BUILD_CLIENT)
    add_executable(ntype_sandbox sandbox/main.cpp)
    target_link_libraries(ntype_sandbox PRIVATE ntype_engine)
    target_compile_definitions(ntype_sandbox PRIVATE NTYPE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
endif()

enable_testing()
# Tests are made of assert(): keep them in Release builds too.
function(ntype_add_test name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE "${CMAKE_SOURCE_DIR}")
    target_link_libraries(${name} PRIVATE ${ARGN})
    if(MSVC)
        target_compile_options(${name} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${name} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

ntype_add_test(protocol_test tests/protocol/protocol_test.cpp ntype_protocol)
ntype_add_test(game_test tests/server/game_test.cpp ntype_server_core)
target_compile_definitions(game_test PRIVATE NTYPE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
ntype_add_test(lobby_test tests/server/lobby_test.cpp ntype_server_core)
target_compile_definitions(lobby_test PRIVATE NTYPE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
ntype_add_test(engine_test tests/engine/engine_test.cpp ntype_engine)
target_compile_definitions(engine_test PRIVATE NTYPE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
if(NTYPE_BUILD_CLIENT)
    ntype_add_test(client_network_test tests/client/client_network_test.cpp ntype_client_core ntype_server_core)
endif()
