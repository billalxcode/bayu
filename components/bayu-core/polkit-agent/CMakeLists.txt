find_package(BayuToolkit REQUIRED)

add_executable(bayu-polkit-agent
    main.cpp
    polkitagentlistener.cpp
    polkitagentlistener.h
    authsession.cpp
    authsession.h
    dialog.cpp
    dialog.h
)
set_target_properties(bayu-polkit-agent PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

target_link_libraries(bayu-polkit-agent PRIVATE Bayu::Toolkit)

install(TARGETS bayu-polkit-agent
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Runtime
)
