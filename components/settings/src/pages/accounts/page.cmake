find_package(BayuClients REQUIRED COMPONENTS accounts)
find_library(SETTINGS_CRYPT_LIBRARY crypt REQUIRED)
target_link_libraries(settings-ui PUBLIC Bayu::AccountsClient ${SETTINGS_CRYPT_LIBRARY})
