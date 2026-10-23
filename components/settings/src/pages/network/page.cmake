find_package(BayuClients REQUIRED COMPONENTS networkmanager)
target_link_libraries(settings-ui PUBLIC Bayu::NetworkManagerClient)
