find_package(BayuClients REQUIRED COMPONENTS display)
target_link_libraries(settings-ui PUBLIC Bayu::DisplayClient)
