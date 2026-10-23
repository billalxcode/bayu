find_package(BayuClients REQUIRED COMPONENTS appearance)
target_link_libraries(settings-ui PUBLIC Bayu::AppearanceClient)
