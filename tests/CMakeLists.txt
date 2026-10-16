find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(ntype_tests
    engine/ecs/RegistryTest.cpp
    engine/events/EventBusTest.cpp
    engine/math/RandomTest.cpp
    engine/net/ByteBufferTest.cpp
    engine/net/NetworkThreadTest.cpp
    engine/net/PacketTest.cpp
    engine/net/ReliableChannelTest.cpp
    engine/scene/StateMachineTest.cpp
    engine/time/GameLoopTest.cpp
    rtype_common/GameDataTest.cpp
    rtype_common/protocol/CodecTest.cpp
    rtype_common/systems/SimulationTest.cpp
)
target_link_libraries(ntype_tests PRIVATE ntype::rtype GTest::gtest GTest::gtest_main)
target_compile_definitions(ntype_tests PRIVATE NTYPE_TEST_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
ntype_set_warnings(ntype_tests)

if(NTYPE_BUILD_SERVER)
    target_sources(ntype_tests PRIVATE server/ServerTest.cpp)
    target_link_libraries(ntype_tests PRIVATE r-type_server_core)
endif()

gtest_discover_tests(ntype_tests DISCOVERY_MODE PRE_TEST)
