cmake_minimum_required(VERSION 3.16)

project(bayu-store
    VERSION 0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

find_package(BayuToolkit REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(FLATPAK REQUIRED IMPORTED_TARGET flatpak)

add_library(store-ui OBJECT
    src/cache.cpp
    src/catalog.cpp
    src/fdroid.cpp
    src/flathub.cpp
    src/flatpak_client.cpp
    src/lapis.cpp
    src/net.cpp
    src/mock_backend.cpp
    src/real_backend.cpp
    src/ui/pages.cpp
    src/ui/store_widgets.cpp
    src/ui/store_window.cpp
    src/ui/style.cpp
    src/ui/widgets.cpp
)
target_include_directories(store-ui PUBLIC src)
target_link_libraries(store-ui PUBLIC Bayu::Toolkit PkgConfig::CURL PkgConfig::FLATPAK)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE store-ui)

add_executable(store-fdroid-test tests/fdroid_test.cpp)
target_link_libraries(store-fdroid-test PRIVATE store-ui)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS data/android-setup DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/bayu-store)
install(FILES data/flathub.flatpakrepo DESTINATION ${CMAKE_INSTALL_DATADIR}/bayu-store)
install(FILES data/org.bayu.store.policy DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
install(FILES data/bayu-store.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
