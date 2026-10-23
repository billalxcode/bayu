find_package(BayuClients REQUIRED COMPONENTS upower powerprofiles)
target_link_libraries(settings-ui PUBLIC Bayu::UPowerClient Bayu::PowerProfilesClient)
