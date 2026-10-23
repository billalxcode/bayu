find_package(BayuClients REQUIRED COMPONENTS audio)
target_link_libraries(settings-ui PUBLIC Bayu::AudioClient)
