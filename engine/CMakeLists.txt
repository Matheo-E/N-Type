# ntype-engine: game-agnostic core. Links Asio privately and never exposes
# SFML or Asio types in its public headers.
find_package(asio CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(ntype-engine STATIC
    src/Version.cpp
    src/ecs/Registry.cpp
    src/ecs/SystemScheduler.cpp
    src/events/EventBus.cpp
    src/math/Random.cpp
    src/net/Endpoint.cpp
    src/net/NetworkThread.cpp
    src/net/Packet.cpp
    src/net/ReliableChannel.cpp
    src/net/UdpSocket.cpp
    src/scene/StateMachine.cpp
    src/time/GameLoop.cpp
    src/util/Log.cpp
)
add_library(ntype::engine ALIAS ntype-engine)

target_include_directories(ntype-engine PUBLIC include)
target_link_libraries(ntype-engine
    PUBLIC Threads::Threads
    PRIVATE asio::asio)
target_compile_definitions(ntype-engine PRIVATE ASIO_NO_DEPRECATED)
if(WIN32)
    # Asio needs to know the minimum Windows version it targets (Windows 10).
    target_compile_definitions(ntype-engine PRIVATE _WIN32_WINNT=0x0A00)
    target_link_libraries(ntype-engine PRIVATE ws2_32 mswsock)
endif()
ntype_set_warnings(ntype-engine)

if(NTYPE_SFML_BACKEND)
    add_subdirectory(backends/sfml)
endif()
