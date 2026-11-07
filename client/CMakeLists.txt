# The network session, without any window code: shared by the executable and the tests.
add_library(ntype_client_core STATIC
    src/ClientNetwork.cpp
)
target_include_directories(ntype_client_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ntype_client_core PUBLIC ntype_shared ntype_engine)

add_executable(r-type_client
    src/main.cpp
    src/Launcher.cpp
    src/ControlsMenu.cpp
    src/ClientApp.cpp
    src/ExplosionEffects.cpp
    src/ImpactSounds.cpp
    src/Hud.cpp
    src/ResultsScreen.cpp
)
target_link_libraries(r-type_client PRIVATE ntype_client_core)
if(MSVC)
    # The Windows vcpkg triplet provides SFML as shared libraries, so its
    # private OpenAL dependency is not exported to consumers.
    target_link_libraries(r-type_client PRIVATE sfml-audio)
else()
    target_link_libraries(r-type_client PRIVATE
        "$<LINK_GROUP:RESCAN,sfml-audio,OpenAL::OpenAL,fmt::fmt>")
endif()
target_compile_definitions(r-type_client PRIVATE
    NTYPE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

foreach(target ntype_client_core r-type_client)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endforeach()
