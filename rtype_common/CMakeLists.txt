# rtype_common: R-Type game layer shared by the client and the server
# (components, systems, protocol). Links the engine, never SFML.
add_library(rtype_common STATIC
    src/GameInfo.cpp
)
add_library(ntype::rtype ALIAS rtype_common)

target_include_directories(rtype_common PUBLIC include)
target_link_libraries(rtype_common PUBLIC ntype::engine)
ntype_set_warnings(rtype_common)
