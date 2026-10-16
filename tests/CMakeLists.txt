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
)
target_link_libraries(ntype_tests PRIVATE ntype::rtype GTest::gtest GTest::gtest_main)
ntype_set_warnings(ntype_tests)

gtest_discover_tests(ntype_tests DISCOVERY_MODE PRE_TEST)
