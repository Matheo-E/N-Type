add_executable(r-type_server
    src/main.cpp
    src/NetworkThread.cpp
    src/Server.cpp
)
target_link_libraries(r-type_server PRIVATE ntype_shared ntype_engine)

if(MSVC)
    target_compile_options(r-type_server PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(r-type_server PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
