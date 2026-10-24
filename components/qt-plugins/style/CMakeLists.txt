project(bayustyleplugin)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_library(bayustyle MODULE
    main.cpp
    bayustyle.h
    bayustyle.cpp
)
target_compile_definitions(bayustyle PRIVATE QT_NO_FOREACH)
target_link_libraries(bayustyle PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

find_program(QT_PATHS_EXECUTABLE NAMES qtpaths6 REQUIRED)
execute_process(COMMAND "${QT_PATHS_EXECUTABLE}" -query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE QT_PLUGINS_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
install(TARGETS bayustyle LIBRARY DESTINATION "${QT_PLUGINS_DIR}/styles")

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
