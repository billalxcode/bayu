find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(packagekit-client-tests packagekit_test.cpp)
target_link_libraries(packagekit-client-tests PRIVATE Bayu::PackageKitClient GTest::gtest_main)
# Each case owns org.freedesktop.PackageKit on the shared session bus.
gtest_discover_tests(packagekit-client-tests PROPERTIES RESOURCE_LOCK packagekit-session-bus)
