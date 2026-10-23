project(bayu-appruntime)
set(TARGET bayu-appruntime)

pkg_check_modules(Systemd REQUIRED IMPORTED_TARGET libsystemd)

set(SOURCES
    applicationscope.cpp
    applicationscope.h
    appruntime.cpp
    appruntime.h
    chromiumswitches.cpp
    chromiumswitches.h
    main.cpp
)

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET}
    bayu-core-common
    Bayu::ToolkitBase
    PkgConfig::Systemd
)

install(TARGETS ${TARGET} DESTINATION ${CMAKE_INSTALL_BINDIR})
