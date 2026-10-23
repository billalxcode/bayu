find_package(BayuClients REQUIRED COMPONENTS upower)
target_link_libraries(settings-ui PUBLIC Bayu::UPowerClient)
