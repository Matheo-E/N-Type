add_executable(r-type_server src/main.cpp)
target_link_libraries(r-type_server PRIVATE ntype_shared)

if(MSVC)
    target_compile_options(r-type_server PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(r-type_server PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
