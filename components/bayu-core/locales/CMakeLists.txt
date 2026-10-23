project(bayu-locales)
set(TARGET bayu-locales)

set(SOURCES
    localesservice.cpp
    localesservice.h
    main.cpp
)

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET}
    Bayu::ToolkitBase
    bayu-core-common
)

configure_file(org.bayu.Locales.service.in org.bayu.Locales.service @ONLY)
configure_file(bayu-locales.service.in bayu-locales.service @ONLY)

install(TARGETS ${TARGET} DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(FILES org.bayu.Locales.conf DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/dbus-1/system.d)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.bayu.Locales.service
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/dbus-1/system-services)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bayu-locales.service DESTINATION lib/systemd/system)
install(FILES org.bayu.locales.policy DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/polkit-1/actions)
