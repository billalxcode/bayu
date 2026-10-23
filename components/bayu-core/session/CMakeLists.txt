project(bayu-session)
set(TARGET bayu-session)

pkg_check_modules(Systemd REQUIRED IMPORTED_TARGET libsystemd)

set(SOURCES
    application.cpp
    autostartentries.cpp
    journalstream.cpp
    main.cpp
    processmanager.cpp
    restartpolicy.cpp
    compositorprocess.cpp
    xwaylandenvironment.cpp
    shellprocess.cpp
    shellconnection.cpp
    power.cpp
    userdirectories.cpp
)

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET}
        bayu-core-common
        Bayu::ToolkitBase
        PkgConfig::Systemd
)

install(TARGETS ${TARGET} DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS bayu-wayland-session DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES document-portal-stop.conf
        DESTINATION /usr/lib/systemd/user/xdg-document-portal.service.d
        RENAME bayu-session.conf)
