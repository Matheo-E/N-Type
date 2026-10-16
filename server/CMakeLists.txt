# Server logic is a library so the tests can drive game instances directly.
add_library(r-type_server_core STATIC
    src/GameInstance.cpp
    src/RoomManager.cpp
    src/ServerApp.cpp
    src/ServerConfig.cpp
    src/SnapshotBuilder.cpp
)
target_include_directories(r-type_server_core PUBLIC src)
target_link_libraries(r-type_server_core PUBLIC ntype::rtype)
ntype_set_warnings(r-type_server_core)

add_executable(r-type_server src/main.cpp)
target_link_libraries(r-type_server PRIVATE r-type_server_core)
ntype_set_warnings(r-type_server)
