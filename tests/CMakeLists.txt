find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(ntype_tests
    engine/VersionTest.cpp
)
target_link_libraries(ntype_tests PRIVATE ntype::rtype GTest::gtest GTest::gtest_main)
ntype_set_warnings(ntype_tests)

gtest_discover_tests(ntype_tests DISCOVERY_MODE PRE_TEST)
