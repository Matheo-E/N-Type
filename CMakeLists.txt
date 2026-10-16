cmake_minimum_required(VERSION 3.25)

project(NType
    VERSION 0.1.0
    DESCRIPTION "R-Type networked shoot'em up and its reusable engine"
    LANGUAGES CXX)

option(NTYPE_BUILD_CLIENT "Build r-type_client (requires SFML)" ON)
option(NTYPE_BUILD_SERVER "Build r-type_server" ON)
option(NTYPE_BUILD_TESTS "Build the unit tests" ON)
option(NTYPE_SFML_BACKEND "Build the SFML backend of the engine" ON)
option(NTYPE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(NTYPE_BUILD_CLIENT AND NOT NTYPE_SFML_BACKEND)
    message(FATAL_ERROR "NTYPE_BUILD_CLIENT requires NTYPE_SFML_BACKEND")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Binaries land at the root of the build tree so they find assets/ next to them.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(NTypeWarnings)
include(NTypeTooling)

add_subdirectory(engine)
add_subdirectory(rtype_common)

if(NTYPE_BUILD_SERVER)
    add_subdirectory(server)
endif()

if(NTYPE_BUILD_CLIENT)
    add_subdirectory(client)
endif()

if(NTYPE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
