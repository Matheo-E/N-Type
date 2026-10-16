add_executable(r-type_client
    src/ClientApp.cpp
    src/ClientConfig.cpp
    src/ClientNetwork.cpp
    src/InputMapper.cpp
    src/Starfield.cpp
    src/WorldSync.cpp
    src/main.cpp
    src/states/ConnectingState.cpp
    src/states/PlayState.cpp
    src/systems/InterpolationSystem.cpp
    src/systems/RenderSystem.cpp
)
target_link_libraries(r-type_client PRIVATE ntype::rtype ntype::engine-sfml)
ntype_set_warnings(r-type_client)
