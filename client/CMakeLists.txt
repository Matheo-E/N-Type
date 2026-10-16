add_executable(r-type_client
    src/main.cpp
)
target_link_libraries(r-type_client PRIVATE ntype::rtype ntype::engine-sfml)
ntype_set_warnings(r-type_client)
