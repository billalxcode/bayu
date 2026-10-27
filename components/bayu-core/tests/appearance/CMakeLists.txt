cmake_minimum_required(VERSION 3.22)
project(dynamic_wallpaper_test LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(BayuToolkit REQUIRED)
find_package(GTest REQUIRED)
set(SERVICE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../bayu-services)
add_executable(dynamic_wallpaper_test dynamic_wallpaper_test.cpp
    ${SERVICE_DIR}/appearance/dynamicwallpaper.cpp
    ${SERVICE_DIR}/appearance/wallpapermanager.cpp
    ${SERVICE_DIR}/schedule/scheduleutils.cpp)
target_include_directories(dynamic_wallpaper_test PRIVATE ${SERVICE_DIR}/appearance ${SERVICE_DIR}/schedule)
target_link_libraries(dynamic_wallpaper_test PRIVATE Bayu::ToolkitBase GTest::gtest_main)
enable_testing()
add_test(NAME dynamic_wallpaper_test COMMAND dynamic_wallpaper_test)
