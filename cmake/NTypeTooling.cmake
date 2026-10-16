# Developer tooling targets: `format`, `format-check` and `tidy`.
# They are only created when the tools are installed, so a machine without
# them can still build the project.

file(GLOB_RECURSE NTYPE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/engine/*.hpp ${CMAKE_SOURCE_DIR}/engine/*.cpp
    ${CMAKE_SOURCE_DIR}/rtype_common/*.hpp ${CMAKE_SOURCE_DIR}/rtype_common/*.cpp
    ${CMAKE_SOURCE_DIR}/server/*.hpp ${CMAKE_SOURCE_DIR}/server/*.cpp
    ${CMAKE_SOURCE_DIR}/client/*.hpp ${CMAKE_SOURCE_DIR}/client/*.cpp
    ${CMAKE_SOURCE_DIR}/tests/*.hpp ${CMAKE_SOURCE_DIR}/tests/*.cpp)

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${NTYPE_SOURCES}
        COMMENT "Formatting sources with clang-format"
        VERBATIM)
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${NTYPE_SOURCES}
        COMMENT "Checking source formatting"
        VERBATIM)
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    # Headers are checked through the translation units that include them.
    set(NTYPE_TRANSLATION_UNITS ${NTYPE_SOURCES})
    list(FILTER NTYPE_TRANSLATION_UNITS INCLUDE REGEX "\\.cpp$")
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} --quiet ${NTYPE_TRANSLATION_UNITS}
        COMMENT "Running clang-tidy"
        VERBATIM)
endif()
