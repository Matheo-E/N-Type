# rtype_common: R-Type game layer shared by the client and the server
# (components, systems, protocol). Links the engine, never SFML.
find_package(nlohmann_json CONFIG REQUIRED)

add_library(rtype_common STATIC
    src/Assets.cpp
    src/EntityFactory.cpp
    src/GameData.cpp
    src/protocol/Codec.cpp
    src/protocol/Transport.cpp
    src/systems/Systems.cpp
)
add_library(ntype::rtype ALIAS rtype_common)

target_include_directories(rtype_common PUBLIC include)
target_link_libraries(rtype_common
    PUBLIC ntype::engine
    PRIVATE nlohmann_json::nlohmann_json)
ntype_set_warnings(rtype_common)
