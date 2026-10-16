add_executable(r-type_server
    src/main.cpp
)
target_link_libraries(r-type_server PRIVATE ntype::rtype)
ntype_set_warnings(r-type_server)
