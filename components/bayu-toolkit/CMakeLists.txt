cmake_minimum_required(VERSION 3.22)

project(bayu-toolkit
    VERSION 1.0
    DESCRIPTION "Bayu UI toolkit"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
# The suite needs GoogleTest, so it is built on request (-DBUILD_TESTING=ON).
option(BUILD_TESTING "Build the tests" OFF)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET wayland-client>=1.23 wayland-egl)
pkg_check_modules(Egl REQUIRED IMPORTED_TARGET egl)
pkg_check_modules(GlesV2 REQUIRED IMPORTED_TARGET glesv2)
pkg_check_modules(Fontconfig REQUIRED IMPORTED_TARGET fontconfig)
pkg_check_modules(Xkbcommon REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(Systemd REQUIRED IMPORTED_TARGET libsystemd)
pkg_check_modules(Icu REQUIRED IMPORTED_TARGET icu-uc icu-i18n)
# Decoders for the image formats Skia leaves out.
pkg_check_modules(ImageCodecs REQUIRED IMPORTED_TARGET
    libpng libwebp libwebpdemux libtiff-4 libheif)
# JPEG XL is opened on first use (src/ui/jxl_library.cpp), by the names the
# libraries give themselves.
pkg_check_modules(Jxl REQUIRED libjxl libjxl_threads)
foreach(library jxl jxl_threads)
    find_library(BAYU_${library}_PATH NAMES ${library} HINTS ${Jxl_LIBRARY_DIRS} REQUIRED)
    execute_process(COMMAND ${CMAKE_OBJDUMP} -p ${BAYU_${library}_PATH} OUTPUT_VARIABLE dynamic)
    if(NOT dynamic MATCHES "SONAME +([^\n]+)")
        message(FATAL_ERROR "No SONAME in ${BAYU_${library}_PATH}")
    endif()
    set(BAYU_${library}_SONAME ${CMAKE_MATCH_1})
endforeach()
pkg_check_modules(SkiaDependencies REQUIRED IMPORTED_TARGET
    freetype2 harfbuzz icu-uc libpng libjpeg zlib expat)
pkg_check_modules(WaylandProtocols REQUIRED wayland-protocols>=1.39)
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)

include(cmake/Skia.cmake)
include(cmake/BayuTranslations.cmake)

add_subdirectory(src)
add_subdirectory(examples)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

bayu_add_translations(bayu-toolkit SOURCES src/base src/ui)

configure_package_config_file(cmake/BayuToolkitConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/BayuToolkitConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BayuToolkit
)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/BayuToolkitConfigVersion.cmake
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/BayuToolkitConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/BayuToolkitConfigVersion.cmake
    cmake/BayuTranslations.cmake
    cmake/BayuTranslationsUpdate.cmake
    cmake/BayuTranslationsCheck.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BayuToolkit
)
