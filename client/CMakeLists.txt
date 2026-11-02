# Everything but the window code, shared by the executable and the tests.
add_library(ntype_client_core STATIC
    src/ClientNetwork.cpp
    src/ui/Theme.cpp
    src/ui/Widgets.cpp
)
target_include_directories(ntype_client_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ntype_client_core PUBLIC ntype_shared ntype_engine)

add_executable(r-type_client
    src/main.cpp
    src/ClientApp.cpp
    src/screens/ConnectScreen.cpp
    src/screens/CreateScreen.cpp
    src/screens/GameScreen.cpp
    src/screens/LobbyListScreen.cpp
)
target_link_libraries(r-type_client PRIVATE ntype_client_core)

foreach(target ntype_client_core r-type_client)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endforeach()
