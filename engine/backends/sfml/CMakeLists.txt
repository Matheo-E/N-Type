# ntype-engine-sfml: implements the engine platform interfaces with SFML.
# Only the client links it; the server stays free of any graphics dependency.
find_package(SFML 3 CONFIG REQUIRED COMPONENTS Graphics Window Audio System)

add_library(ntype-engine-sfml STATIC
    src/KeyMapping.cpp
    src/SfmlAudio.cpp
    src/SfmlInput.cpp
    src/SfmlRenderer.cpp
    src/SfmlWindow.cpp
)
add_library(ntype::engine-sfml ALIAS ntype-engine-sfml)

target_include_directories(ntype-engine-sfml PUBLIC include)
target_link_libraries(ntype-engine-sfml
    PUBLIC ntype::engine SFML::Graphics SFML::Window SFML::Audio SFML::System)
ntype_set_warnings(ntype-engine-sfml)
