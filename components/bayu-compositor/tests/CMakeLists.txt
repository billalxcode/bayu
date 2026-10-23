find_package(GTest)
if(NOT GTest_FOUND)
    message(FATAL_ERROR "BUILD_TESTING=ON needs GoogleTest (Debian: libgtest-dev)")
endif()
pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET wayland-client)
find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)

# loop_test_util.h: waiting on the main loop in every test.
add_library(bayu-test-loop STATIC loop_test_util.cpp)
target_link_libraries(bayu-test-loop PUBLIC bayu-core)

# bayu_add_test(name [DBUS] libraries...)
# A GoogleTest binary on gtest_toolkit_main.cpp, whose main loop runs the
# timers, fd watches, posted tasks and sd-bus connections of the code under
# test while it waits (loop_test_util.h). DBUS runs it on a private session
# bus.
function(bayu_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "DBUS" "" "")
    add_executable(${name} ${name}.cpp gtest_toolkit_main.cpp)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE GTest::gtest bayu-core bayu-test-loop
        ${arg_UNPARSED_ARGUMENTS})
    if(arg_DBUS)
        add_test(NAME ${name} COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:${name}>)
    else()
        add_test(NAME ${name} COMMAND ${name})
    endif()
endfunction()

bayu_add_test(main_loop_test bayu-toolkit-host)
bayu_add_test(core_test)
target_sources(core_test PRIVATE ${PROJECT_SOURCE_DIR}/src/core/compositor_options.cpp)
target_compile_definitions(core_test PRIVATE
    BAYU_PORTAL_BACKEND="${CMAKE_INSTALL_FULL_LIBEXECDIR}/xdg-desktop-portal-bayu"
    BAYU_SCREEN_RECORDER="${CMAKE_INSTALL_FULL_LIBEXECDIR}/bayu-screenrecorder"
    BAYU_BINDIR="${CMAKE_INSTALL_FULL_BINDIR}")

bayu_add_test(image_capture_test bayu-compositor-core bayu-session PkgConfig::WaylandClient)
bayu_add_test(serial_tracker_test bayu-wayland)
bayu_add_test(client_tracker_test bayu-wayland)
bayu_add_test(wayland_server_test bayu-wayland PkgConfig::WaylandClient)
bayu_add_test(scene_test bayu-scene)
bayu_add_test(shm_pool_test bayu-compositor-core)
bayu_add_test(buffer_test bayu-compositor-core)
bayu_add_test(surface_test bayu-compositor-core)
bayu_add_test(wayland_protocol_test bayu-compositor-core bayu-wayland-dmabuf
    PkgConfig::WaylandClient)
bayu_add_test(xkb_tracker_test bayu-input)
bayu_add_test(key_translator_test bayu-input)
bayu_add_test(input_test bayu-input)
bayu_add_test(computer_control_test bayu-computer-control)
bayu_add_test(remembered_apps_test bayu-computer-control)
bayu_add_test(touch_test bayu-input)
foreach(test pointer_acceleration_test touchpad_click_test)
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE bayu-mouse-settings GTest::gtest_main)
    add_test(NAME ${test} COMMAND ${test})
endforeach()
add_executable(mouse_settings_test mouse_settings_test.cpp)
target_include_directories(mouse_settings_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mouse_settings_test PRIVATE bayu-mouse-settings GTest::gtest_main)
add_test(NAME mouse_settings_test COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:mouse_settings_test>)
add_executable(touchpad_settings_test touchpad_settings_test.cpp)
target_include_directories(touchpad_settings_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(touchpad_settings_test PRIVATE bayu-mouse-settings GTest::gtest_main)
add_test(NAME touchpad_settings_test COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:touchpad_settings_test>)
add_executable(touchscreen_settings_test touchscreen_settings_test.cpp)
target_include_directories(touchscreen_settings_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(touchscreen_settings_test PRIVATE bayu-mouse-settings GTest::gtest_main)
add_test(NAME touchscreen_settings_test COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:touchscreen_settings_test>)
add_executable(keyboard_settings_test keyboard_settings_test.cpp)
target_include_directories(keyboard_settings_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(keyboard_settings_test PRIVATE bayu-keyboard-settings GTest::gtest_main)
add_test(NAME keyboard_settings_test COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:keyboard_settings_test>)
bayu_add_test(drm_selection_test bayu-backend)
bayu_add_test(drm_properties_test bayu-backend)
bayu_add_test(drm_configuration_planner_test bayu-fake-drm)
bayu_add_test(drm_multi_device_planner_test bayu-backend)
bayu_add_test(drm_hotplug_monitor_test bayu-backend)
bayu_add_test(libseat_session_test bayu-backend)

# Manual wl_shm client. Not registered with ctest: it needs a running
# compositor and a screen to look at.
add_executable(bayu-shm-demo manual/shm_client.cpp)
target_link_libraries(bayu-shm-demo PRIVATE PkgConfig::WaylandClient)

bayu_add_test(sub_surface_test bayu-compositor-core)
bayu_add_test(wm_test bayu-wm)
bayu_add_test(subsurface_output_protocol_test bayu-compositor-core PkgConfig::WaylandClient)
bayu_add_test(seat_protocol_test bayu-wayland-input bayu-wm
    PkgConfig::WaylandClient PkgConfig::Xkbcommon)
bayu_add_test(xdg_positioner_test bayu-wayland-shell)
bayu_add_test(shell_surface_test bayu-decoration)
bayu_add_test(decoration_test bayu-decoration)
bayu_add_test(text_test bayu-decoration PkgConfig::TitleText)
bayu_add_test(xdg_shell_protocol_test bayu-wayland-shell bayu-wayland-input
    PkgConfig::WaylandClient)
bayu_add_test(protocol_binding_test bayu-wayland-globals PkgConfig::WaylandClient)
set_tests_properties(protocol_binding_test PROPERTIES TIMEOUT 60)
# Seeded random request sequences against every global; with clang,
# -DBAYU_LIBFUZZER=ON builds it for libFuzzer and AddressSanitizer instead.
option(BAYU_LIBFUZZER "Build protocol_fuzzer for libFuzzer" OFF)
add_executable(protocol_fuzzer protocol_fuzzer.cpp)
target_include_directories(protocol_fuzzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(protocol_fuzzer PRIVATE bayu-wayland-globals PkgConfig::WaylandClient)
if(BAYU_LIBFUZZER)
    target_compile_definitions(protocol_fuzzer PRIVATE BAYU_LIBFUZZER)
    target_compile_options(protocol_fuzzer PRIVATE -fsanitize=fuzzer,address)
    target_link_options(protocol_fuzzer PRIVATE -fsanitize=fuzzer,address)
else()
    add_test(NAME protocol_fuzzer COMMAND protocol_fuzzer)
    set_tests_properties(protocol_fuzzer PROPERTIES TIMEOUT 300)
endif()
bayu_add_test(xdg_foreign_protocol_test bayu-wayland-shell bayu-wayland-input
    PkgConfig::WaylandClient)
bayu_add_test(xdg_toplevel_drag_protocol_test bayu-wayland-shell bayu-wayland-input
    PkgConfig::WaylandClient)
bayu_add_test(foreign_toplevel_list_test bayu-wayland-private-shell bayu-input
    bayu-compositor-core PkgConfig::WaylandClient)
bayu_add_test(private_shell_protocol_test bayu-wayland-private-shell
    bayu-wayland-shell bayu-wayland-input PkgConfig::WaylandClient)
bayu_add_test(work_area_test bayu-wm)
bayu_add_test(dock_layout_test bayu-wm)
bayu_add_test(session_lock_test bayu-session bayu-wm bayu-scene bayu-decoration)
bayu_add_test(accelerator_test DBUS bayu-accelerators bayu-session bayu-wm
    bayu-scene)
bayu_add_test(xdg_decoration_protocol_test bayu-wayland-shell bayu-wayland-input
    bayu-decoration PkgConfig::WaylandClient)
add_executable(bayu-subsurface-demo manual/subsurface_client.cpp)
target_link_libraries(bayu-subsurface-demo PRIVATE PkgConfig::WaylandClient)

# Manual xdg-shell demo. --script runs the whole toplevel state machine
# and exits, which is what the VT harness drives.
add_executable(bayu-xdg-demo manual/xdg_client.cpp)
target_link_libraries(bayu-xdg-demo PRIVATE PkgConfig::WaylandClient bayu-protocols)

# Manual xdg-decoration demo. Draws only its client area, so everything
# around it on screen is the compositor's own frame.
add_executable(bayu-decoration-demo manual/decoration_client.cpp)
target_link_libraries(bayu-decoration-demo
    PRIVATE PkgConfig::WaylandClient bayu-protocols)

# Development harness that plays the session's role for the shell: it obtains a
# privileged Wayland connection from a running compositor and execs a program
# with it. Not registered with ctest; it needs a compositor and a shell.
add_executable(bayu-shell-session-launcher manual/shell_session_launcher.cpp)

# Manual input demo. Needs a running compositor, a screen and a mouse.
add_executable(bayu-input-demo manual/input_client.cpp)
target_link_libraries(bayu-input-demo PRIVATE PkgConfig::WaylandClient PkgConfig::Xkbcommon)
target_compile_definitions(subsurface_output_protocol_test PRIVATE
    SUBSURFACE_DEMO_PATH="$<TARGET_FILE:bayu-subsurface-demo>")
add_dependencies(subsurface_output_protocol_test bayu-subsurface-demo)

bayu_add_test(output_scheduler_test bayu-output-scheduler bayu-compositor-core bayu-decoration)

bayu_add_test(clipboard_protocol_test bayu-wayland-input PkgConfig::WaylandClient)
set_tests_properties(clipboard_protocol_test PROPERTIES TIMEOUT 30)

bayu_add_test(selection_test bayu-input)

bayu_add_test(dmabuf_test bayu-wayland-dmabuf bayu-compositor-core
    PkgConfig::WaylandClient PkgConfig::Libdrm)

bayu_add_test(shadow_test bayu-decoration)
bayu_add_test(shadow_controller_test bayu-decoration)
bayu_add_test(shadow_pixel_test bayu-decoration bayu-render)
set_tests_properties(shadow_pixel_test PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")

bayu_add_test(motion_test bayu-scene)
bayu_add_test(window_animation_test bayu-scene)
bayu_add_test(screen_transition_node_test bayu-scene)
bayu_add_test(desk_switch_node_test bayu-scene)
bayu_add_test(desks_test bayu-wm)
bayu_add_test(overview_layout_test bayu-wm)
bayu_add_test(overview_node_test bayu-scene)
bayu_add_test(overview_test bayu-wm)
bayu_add_test(show_desktop_test bayu-wm)
bayu_add_test(hot_corner_test bayu-accelerators)

bayu_add_test(cursor_test bayu-cursor)
bayu_add_test(output_cursor_router_test bayu-cursor)

# DMA-BUF scanout: eligibility, plane assignment, the atomic commit
# path, explicit synchronization and the hardware cursor.
bayu_add_test(overlay_candidate_test bayu-scene)
bayu_add_test(plane_assignment_test bayu-backend PkgConfig::Libdrm)
bayu_add_test(sync_fence_test bayu-sync bayu-compositor-core)
bayu_add_test(commit_readiness_test bayu-sync bayu-compositor-core)

add_library(bayu-fake-drm STATIC fake_drm_device.cpp fake_drm_device.h)
target_include_directories(bayu-fake-drm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bayu-fake-drm PUBLIC bayu-backend PkgConfig::Libdrm)

bayu_add_test(scanout_test bayu-fake-drm bayu-compositor-core)
bayu_add_test(hardware_cursor_test bayu-fake-drm bayu-cursor bayu-session)

bayu_add_test(window_hints_protocol_test bayu-wayland-shell PkgConfig::WaylandClient)
bayu_add_test(surface_blur_protocol_test bayu-wayland-shell PkgConfig::WaylandClient)

bayu_add_test(dnd_protocol_test bayu-cursor bayu-wayland-input
    bayu-wayland-private-shell PkgConfig::WaylandClient)

bayu_add_test(data_control_protocol_test bayu-wayland-input bayu-wayland-private-shell
    bayu-wayland-shell PkgConfig::WaylandClient)

bayu_add_test(dnd_toolkit_test bayu-wayland-input bayu-wayland-shell PkgConfig::WaylandClient)
target_compile_definitions(dnd_toolkit_test PRIVATE GTK_DND_CLIENT="${CMAKE_CURRENT_SOURCE_DIR}/dnd/gtk_client.py")
set_tests_properties(dnd_toolkit_test PROPERTIES TIMEOUT 70)

set_tests_properties(private_shell_protocol_test PROPERTIES TIMEOUT 60)

bayu_add_test(viewporter_test bayu-compositor-core)
bayu_add_test(viewporter_protocol_test bayu-compositor-core PkgConfig::WaylandClient)
set_tests_properties(viewporter_protocol_test PROPERTIES TIMEOUT 30)
bayu_add_test(viewporter_pixel_test bayu-compositor-core bayu-input bayu-render
    PkgConfig::Gbm PkgConfig::Libdrm)
set_tests_properties(viewporter_pixel_test PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")

bayu_add_test(fractional_scale_protocol_test bayu-compositor-core PkgConfig::WaylandClient)
set_tests_properties(fractional_scale_protocol_test PROPERTIES TIMEOUT 30)

bayu_add_test(fractional_scale_pixel_test bayu-compositor-core bayu-render)
set_tests_properties(fractional_scale_pixel_test PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")

bayu_add_test(output_scale_test bayu-input)
bayu_add_test(display_types_test bayu-display)
bayu_add_test(edid_test bayu-display)
bayu_add_test(display_preferences_test bayu-display)
bayu_add_test(recommended_scale_test bayu-display)
bayu_add_test(display_transform_test bayu-display)
bayu_add_test(mirror_transform_test bayu-display)
bayu_add_test(display_layout_test bayu-display)
bayu_add_test(chromeos_display_layout_test bayu-display)
bayu_add_test(display_configuration_validator_test bayu-display)
bayu_add_test(display_manager_test bayu-display)
bayu_add_test(native_display_delegate_test bayu-display)
target_sources(native_display_delegate_test PRIVATE
    fake_native_display_delegate.cpp fake_native_display_delegate.h)
bayu_add_test(display_configuration_controller_test bayu-display)
target_sources(display_configuration_controller_test PRIVATE
    fake_native_display_delegate.cpp fake_native_display_delegate.h)
bayu_add_test(night_light_test bayu-display)
bayu_add_test(display_config_service_test DBUS bayu-display-ipc)
target_sources(display_config_service_test PRIVATE
    fake_native_display_delegate.cpp fake_native_display_delegate.h)

bayu_add_test(presentation_test bayu-compositor-core)
bayu_add_test(presentation_protocol_test bayu-compositor-core PkgConfig::WaylandClient)
add_executable(bayu-presentation-client manual/presentation_client.cpp)
target_link_libraries(bayu-presentation-client PRIVATE bayu-protocols PkgConfig::WaylandClient)

bayu_add_test(drm_output_test bayu-backend bayu-compositor-core PkgConfig::WaylandClient)
target_sources(drm_output_test PRIVATE fake_drm_device.cpp)
set_tests_properties(drm_output_test PROPERTIES TIMEOUT 60 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")

bayu_add_test(window_visual_test bayu-decoration)
bayu_add_test(window_visual_pixel_test bayu-decoration bayu-render)
set_tests_properties(window_visual_pixel_test PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")
add_executable(toolkit-resize-client visuals/toolkit_client.cpp)
target_link_libraries(toolkit-resize-client PRIVATE Bayu::Toolkit)
# DBUS: the GTK clients activate portal and gvfs services on the session bus.
bayu_add_test(window_visual_toolkit_test DBUS bayu-wayland-input bayu-wayland-shell
    bayu-decoration bayu-render PkgConfig::WaylandClient)
target_compile_definitions(window_visual_toolkit_test PRIVATE
    TOOLKIT_RESIZE_CLIENT="$<TARGET_FILE:toolkit-resize-client>"
    ELECTRON_VISUAL_CLIENT="${CMAKE_CURRENT_SOURCE_DIR}/visuals/electron_client.js")
add_dependencies(window_visual_toolkit_test toolkit-resize-client)
set_tests_properties(window_visual_toolkit_test PROPERTIES TIMEOUT 70
    ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")

if(BAYU_ENABLE_XWAYLAND)
    bayu_add_test(xwayland_test bayu-xwayland bayu-wayland-shell bayu-wayland-input bayu-render bayu-decoration PkgConfig::WaylandClient)
    add_executable(xwayland-startup-fault manual/xwayland_startup_fault.cpp)
    target_compile_definitions(xwayland_test PRIVATE XWAYLAND_FAULT_PATH="$<TARGET_FILE:xwayland-startup-fault>")
    add_executable(x11-test-client xwayland/x11_client.cpp)
    target_link_libraries(x11-test-client PRIVATE PkgConfig::Xcb)
    target_compile_definitions(xwayland_test PRIVATE X11_TEST_CLIENT="$<TARGET_FILE:x11-test-client>"
        GTK_DND_CLIENT="${CMAKE_CURRENT_SOURCE_DIR}/dnd/gtk_client.py")
    add_dependencies(xwayland_test xwayland-startup-fault x11-test-client)
    set_tests_properties(xwayland_test PROPERTIES TIMEOUT 90 ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")
endif()

bayu_add_test(display_runtime_test bayu-display-runtime bayu-session)

bayu_add_test(capture_pixel_test bayu-render)
bayu_add_test(render_internals_test bayu-render PkgConfig::Gbm PkgConfig::Libdrm)
set_tests_properties(render_internals_test PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")
set_tests_properties(capture_pixel_test PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")
bayu_add_test(desk_switch_pixel_test bayu-scene bayu-render)
set_tests_properties(desk_switch_pixel_test PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")
bayu_add_test(overview_pixel_test bayu-scene bayu-render)
set_tests_properties(overview_pixel_test PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")
bayu_add_test(wallpaper_pixel_test bayu-scene bayu-render)
set_tests_properties(wallpaper_pixel_test PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")
set_tests_properties(image_capture_test PROPERTIES TIMEOUT 15)


bayu_add_test(damage_test bayu-compositor-core)
bayu_add_test(damage_pixel_test bayu-scene bayu-render)
set_tests_properties(damage_pixel_test PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")
bayu_add_test(damage_gbm_test bayu-scene bayu-render PkgConfig::Gbm)
bayu_add_test(dmabuf_format_gbm_test bayu-render PkgConfig::Gbm)
bayu_add_test(capture_dmabuf_gbm_test bayu-render PkgConfig::Gbm PkgConfig::Libdrm)

bayu_add_test(idle_notify_protocol_test bayu-wayland-input PkgConfig::WaylandClient)

bayu_add_test(auth_controller_test bayu-auth)
bayu_add_test(switcher_view_test bayu-switcher)
bayu_add_test(shell_test bayu-shell bayu-computer-control bayu-session
    bayu-wayland-input)
target_sources(shell_test PRIVATE ${PROJECT_SOURCE_DIR}/src/core/computer_control_environment.cpp)
bayu_add_test(shell_gpu_pixel_test bayu-shell bayu-render)
set_tests_properties(shell_gpu_pixel_test PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")
# The shell components' memory, step by step; by hand.
add_executable(bayu-shell-memory-bench manual/shell_memory_bench.cpp)
target_include_directories(bayu-shell-memory-bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bayu-shell-memory-bench PRIVATE bayu-shell-osd bayu-shell-dock-view
    bayu-decoration bayu-render bayu-test-loop)
bayu_add_test(panel_gpu_pixel_test bayu-auth bayu-render)
target_include_directories(panel_gpu_pixel_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(panel_gpu_pixel_test PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")
bayu_add_test(switcher_gpu_pixel_test bayu-switcher bayu-render)
set_tests_properties(switcher_gpu_pixel_test PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;EGL_PLATFORM=surfaceless")
bayu_add_test(auth_ui_test DBUS bayu-auth)
bayu_add_test(greetd_backend_test bayu-auth)
bayu_add_test(screen_saver_service_test DBUS bayu-session)
bayu_add_test(applications_service_test DBUS bayu-session)

bayu_add_test(window_debug_service_test DBUS bayu-session)

bayu_add_test(sleep_lock_test DBUS bayu-session)

bayu_add_test(power_button_test DBUS bayu-session)

bayu_add_test(input_method_protocol_test bayu-wayland-input PkgConfig::WaylandClient)
set_tests_properties(input_method_protocol_test PROPERTIES TIMEOUT 45)

add_executable(input_method_settings_test input_method_settings_test.cpp)
target_include_directories(input_method_settings_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(input_method_settings_test PRIVATE bayu-input-method-settings
    GTest::gtest_main)
add_test(NAME input_method_settings_test
    COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:input_method_settings_test>)

add_executable(input_method_launched_client input_method/launched_client.cpp)
target_link_libraries(input_method_launched_client PRIVATE PkgConfig::WaylandClient)
bayu_add_test(input_method_launcher_test DBUS bayu-wayland-input
    bayu-wayland-private-shell PkgConfig::WaylandClient)
target_compile_definitions(input_method_launcher_test PRIVATE
    BAYU_LAUNCHED_CLIENT="$<TARGET_FILE:input_method_launched_client>")
add_dependencies(input_method_launcher_test input_method_launched_client)
set_tests_properties(input_method_launcher_test PROPERTIES TIMEOUT 60)

# Sanitizer builds run several times slower than the per-test timeouts above
# allow for; configure with -DBAYU_TEST_TIMEOUT_SCALE=5 for those.
set(BAYU_TEST_TIMEOUT_SCALE 1 CACHE STRING "Multiplier for every test's TIMEOUT")
if(NOT BAYU_TEST_TIMEOUT_SCALE EQUAL 1)
    get_directory_property(bayu_all_tests TESTS)
    foreach(test ${bayu_all_tests})
        get_test_property(${test} TIMEOUT timeout)
        if(timeout AND NOT timeout STREQUAL "NOTFOUND")
            math(EXPR scaled "${timeout} * ${BAYU_TEST_TIMEOUT_SCALE}")
            set_tests_properties(${test} PROPERTIES TIMEOUT ${scaled})
        endif()
    endforeach()
endif()

add_subdirectory(shell)
