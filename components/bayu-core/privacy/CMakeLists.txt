project(bayu-privacy)
set(TARGET bayu-privacy)

set(SOURCES
    captureswitches.cpp
    captureswitches.h
    main.cpp
    privacyservice.cpp
    privacyservice.h
    usbcameras.cpp
    usbcameras.h
)

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET}
    Bayu::ToolkitBase
    bayu-core-common
)

configure_file(org.bayu.Privacy.service.in org.bayu.Privacy.service @ONLY)
configure_file(bayu-privacy.service.in bayu-privacy.service @ONLY)

install(TARGETS ${TARGET} DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(FILES org.bayu.Privacy.conf DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/dbus-1/system.d)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.bayu.Privacy.service
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/dbus-1/system-services)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bayu-privacy.service DESTINATION lib/systemd/system)
install(FILES org.bayu.privacy.policy DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/polkit-1/actions)
install(FILES 60-bayu-privacy.rules DESTINATION lib/udev/rules.d)
