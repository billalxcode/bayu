find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(style-test
    style_test.cpp
    ../bayustyle.cpp
)
target_include_directories(style-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(style-test PRIVATE Qt6::Widgets Qt6::Test)
add_test(NAME style-test COMMAND style-test)
set_tests_properties(style-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
