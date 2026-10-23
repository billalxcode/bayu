add_executable(calendar-core-tests core_test.cpp)
target_link_libraries(calendar-core-tests PRIVATE calendar-core GTest::gtest_main)
add_test(NAME calendar_core COMMAND calendar-core-tests)

add_executable(calendar-ui-tests ui_test.cpp)
target_link_libraries(calendar-ui-tests PRIVATE calendar-ui GTest::gtest)
add_test(NAME calendar_ui COMMAND calendar-ui-tests)

add_executable(calendar-dialogs-tests dialogs_test.cpp)
target_link_libraries(calendar-dialogs-tests PRIVATE calendar-ui GTest::gtest)
add_test(NAME calendar_dialogs COMMAND calendar-dialogs-tests)

add_executable(calendar-capture capture.cpp)
target_link_libraries(calendar-capture PRIVATE calendar-ui)

add_executable(calendar-reminders-tests reminders_test.cpp ${PROJECT_SOURCE_DIR}/src/reminderservice.cpp)
target_link_libraries(calendar-reminders-tests PRIVATE calendar-core GTest::gtest_main)
add_test(NAME calendar_reminders COMMAND calendar-reminders-tests)
