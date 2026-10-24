cmake_minimum_required(VERSION 3.16)

project(nabau
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CTest)

find_package(BayuToolkit REQUIRED)
find_package(EXPAT REQUIRED)
find_library(CRYPT_LIBRARY crypt REQUIRED)

file(GLOB page_sources CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/ui/pages/*.cpp)

add_library(installer-ui OBJECT
    src/anaconda_backend.cpp
    src/mock_backend.cpp
    src/network_manager.cpp
    src/ui/disk_editor.cpp
    src/ui/installer.cpp
    src/ui/style.cpp
    src/ui/widgets.cpp
    ${page_sources}
)
target_include_directories(installer-ui PUBLIC src)
target_link_libraries(installer-ui PUBLIC Bayu::Toolkit EXPAT::EXPAT ${CRYPT_LIBRARY})

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE installer-ui)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS data/nabau-backend data/post-install
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/nabau)
# Anaconda serves its "web UI", the installer, when this exists.
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_DATADIR}/cockpit/anaconda-webui)
install(FILES data/org.bayu.nabau.policy DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
install(FILES data/50-nabau.rules DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/rules.d)
install(FILES data/nabau.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/nabau-autostart.desktop DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart
    RENAME nabau.desktop)
