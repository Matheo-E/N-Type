add_executable(r-type_client
    src/main.cpp
    src/Launcher.cpp
    src/ClientApp.cpp
    src/ClientNetwork.cpp
)
target_link_libraries(r-type_client PRIVATE ntype_shared ntype_engine)
if(MSVC)
    # link.exe resolves static libraries without GNU-style archive groups.
    target_link_libraries(r-type_client PRIVATE
        sfml-audio OpenAL::OpenAL fmt::fmt)
else()
    target_link_libraries(r-type_client PRIVATE
        "$<LINK_GROUP:RESCAN,sfml-audio,OpenAL::OpenAL,fmt::fmt>")
endif()
target_compile_definitions(r-type_client PRIVATE
    NTYPE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

if(MSVC)
    target_compile_options(r-type_client PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(r-type_client PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
