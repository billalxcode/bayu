cmake_minimum_required(VERSION 3.22)
project(core)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(BayuToolkit REQUIRED)

include(GNUInstallDirs)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(common)
add_subdirectory(appruntime)
add_subdirectory(locales)
add_subdirectory(privacy)
add_subdirectory(session)
add_subdirectory(screenrecorder)
add_subdirectory(polkit-agent)
add_subdirectory(bayu-services)

bayu_add_translations(bayu-core SOURCES bayu-services session
                          DESKTOP_FILES session/bayu-wayland.desktop)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bayu-wayland.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/wayland-sessions/)
