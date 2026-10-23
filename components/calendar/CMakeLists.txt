cmake_minimum_required(VERSION 3.16)

project(bayu-calendar VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CTest)

find_package(BayuToolkit REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBICAL REQUIRED IMPORTED_TARGET libical)
pkg_check_modules(ICU REQUIRED IMPORTED_TARGET icu-uc icu-i18n)
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)

add_library(calendar-core STATIC
    src/calendarmodel.cpp
    src/calendarstore.cpp
    src/color.cpp
    src/datetime.cpp
    src/download.cpp
    src/eventfile.cpp
    src/eventlayout.cpp
    src/holidayfeeds.cpp
    src/localedata.cpp
    src/reminders.cpp
    src/sha1.cpp
    src/subscriptions.cpp
    src/text.cpp
    src/timezones.cpp
    src/trash.cpp
    src/userdirs.cpp
    src/uuid.cpp
)
target_include_directories(calendar-core PUBLIC src)
target_link_libraries(calendar-core PUBLIC Bayu::ToolkitBase PkgConfig::LIBICAL PkgConfig::ICU PkgConfig::CURL)

add_library(calendar-ui STATIC
    src/ui/calendarroot.cpp
    src/ui/calendarwindow.cpp
    src/ui/colors.cpp
    src/ui/dialogs.cpp
    src/ui/eventdialog.cpp
    src/ui/eventviews.cpp
    src/ui/glyphs.cpp
    src/ui/headerbar.cpp
    src/ui/minimonth.cpp
    src/ui/monthview.cpp
    src/ui/searchresults.cpp
    src/ui/sidebar.cpp
    src/ui/timegridview.cpp
    src/ui/yearview.cpp
)
target_link_libraries(calendar-ui PUBLIC calendar-core Bayu::Toolkit)

add_executable(bayu-calendar src/main.cpp)
target_compile_definitions(bayu-calendar PRIVATE CALENDAR_VERSION="${PROJECT_VERSION}")
target_link_libraries(bayu-calendar PRIVATE calendar-ui)

add_executable(bayu-calendar-reminders
    src/remindersmain.cpp
    src/reminderservice.cpp
)
target_link_libraries(bayu-calendar-reminders PRIVATE calendar-core)

install(TARGETS bayu-calendar bayu-calendar-reminders RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

bayu_add_translations(bayu-calendar SOURCES src
                          DESKTOP_FILES bayu-calendar.desktop bayu-calendar-reminders.desktop)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bayu-calendar.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bayu-calendar-reminders.desktop
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    add_subdirectory(tests)
endif()
