# Clients of system services, one library per directory, linking only
# the toolkit's base library. They form the BayuClients package; a
# consumer asks for them by directory name:
#   find_package(BayuClients REQUIRED COMPONENTS appearance)
#   target_link_libraries(app PRIVATE Bayu::AppearanceClient)

find_package(BayuToolkit REQUIRED)

set(BAYU_FRAMEWORK_CLIENTS "all" CACHE STRING
    "The clients/ directories to build: all, or a list of their names")

# bayu_add_client(<name> TARGET <Target> SOURCES ... HEADERS ... [LIBRARIES ...])
# builds libbayu-<name>-client as Bayu::<Target>, its headers included
# as <bayu/<name>/...>.
function(bayu_add_client name)
    cmake_parse_arguments(arg "" "TARGET" "SOURCES;HEADERS;LIBRARIES" ${ARGN})
    bayu_framework_add_library(bayu-${name}-client
        TARGET ${arg_TARGET}
        MODULE ${name}
        EXPORT BayuClient${name}Targets
        SOURCES ${arg_SOURCES}
        HEADERS ${arg_HEADERS}
        PUBLIC_LIBRARIES Bayu::ToolkitBase ${arg_LIBRARIES}
    )
    bayu_framework_export(BayuClients BayuClient${name}Targets)
endfunction()

file(GLOB client_directories LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/*)
foreach(directory ${client_directories})
    get_filename_component(name ${directory} NAME)
    if(NOT EXISTS ${directory}/CMakeLists.txt)
        continue()
    endif()
    if(BAYU_FRAMEWORK_CLIENTS STREQUAL "all" OR name IN_LIST BAYU_FRAMEWORK_CLIENTS)
        add_subdirectory(${name})
    endif()
endforeach()

bayu_framework_add_package(BayuClients)
