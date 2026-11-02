# Game logic without the socket, shared by the server and its tests.
add_library(ntype_server_core STATIC
    src/Game.cpp
    src/Lobby.cpp
    src/Lobbies.cpp
)
target_include_directories(ntype_server_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ntype_server_core PUBLIC ntype_shared ntype_engine)

add_executable(r-type_server
    src/main.cpp
    src/NetworkThread.cpp
    src/Server.cpp
)
target_link_libraries(r-type_server PRIVATE ntype_server_core)

foreach(target ntype_server_core r-type_server)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endforeach()
