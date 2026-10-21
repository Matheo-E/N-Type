add_executable(r-type_client
    src/main.cpp
    src/ClientApp.cpp
    src/ClientNetwork.cpp
)
target_link_libraries(r-type_client PRIVATE ntype_shared SFML::Graphics SFML::Window SFML::System)

if(MSVC)
    target_compile_options(r-type_client PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(r-type_client PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
