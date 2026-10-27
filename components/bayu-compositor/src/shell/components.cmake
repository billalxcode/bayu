# The shell's components drawn in the compositor, one static library each,
# on src/shell (internal windows) and their backends.

# The desktop background: the shared wallpaper decode, one node per output
# and the status bar's backdrop rules.
find_package(BayuClients REQUIRED COMPONENTS appearance)
add_library(bayu-shell-wallpaper STATIC
    src/shell/wallpaper/bar_backdrop.cpp
    src/shell/wallpaper/bar_backdrop.h
    src/shell/wallpaper/wallpaper.cpp
    src/shell/wallpaper/wallpaper.h
    src/shell/wallpaper/wallpaper_cache.cpp
    src/shell/wallpaper/wallpaper_cache.h
)
target_link_libraries(bayu-shell-wallpaper PUBLIC bayu-shell Bayu::AppearanceClient)
target_link_libraries(bayu-compositor PRIVATE bayu-shell-wallpaper)

# The on-screen display: org.bayu.Osd and its level panel.
add_library(bayu-shell-osd STATIC
    src/shell/osd/level_track.cpp
    src/shell/osd/level_track.h
    src/shell/osd/osd.cpp
    src/shell/osd/osd.h
    src/shell/osd/osd_levels.cpp
)
target_link_libraries(bayu-shell-osd PUBLIC bayu-shell bayu-shell-statusbar
    Bayu::AudioClient)
target_link_libraries(bayu-compositor PRIVATE bayu-shell-osd)

# The dock: its view, window and input on the dock's backend.
add_library(bayu-shell-dock-view STATIC
    src/shell/dock/dock_icon.cpp
    src/shell/dock/dock_icon.h
    src/shell/dock/dock_metrics.h
    src/shell/dock/dock_presentation.cpp
    src/shell/dock/dock_presentation.h
    src/shell/dock/dock_tooltip.cpp
    src/shell/dock/dock_tooltip.h
    src/shell/dock/dock_window.cpp
    src/shell/dock/dock_window.h
)
target_link_libraries(bayu-shell-dock-view PUBLIC bayu-shell bayu-shell-dock)
target_link_libraries(bayu-compositor PRIVATE bayu-shell-dock-view)

# The clipboard history's picker on the clipboard backend.
add_library(bayu-shell-clipboard-view STATIC
    src/shell/clipboard/clipboard_picker.cpp
    src/shell/clipboard/clipboard_picker.h
    src/shell/clipboard/clipboard_picker_view.cpp
    src/shell/clipboard/clipboard_picker_view.h
)
target_link_libraries(bayu-shell-clipboard-view PUBLIC bayu-shell bayu-shell-clipboard)
target_link_libraries(bayu-compositor PRIVATE bayu-shell-clipboard-view)

# The sheets offering force quit for applications that stopped responding.
add_library(bayu-shell-notresponding STATIC
    src/shell/notresponding/not_responding_sheets.cpp
    src/shell/notresponding/not_responding_sheets.h
)
target_link_libraries(bayu-shell-notresponding PUBLIC bayu-shell)
target_link_libraries(bayu-compositor PRIVATE bayu-shell-notresponding)
# The screen capture interface: org.bayu.Screenshot, its overlays,
# toolbar and Options, pictures and recordings.
add_library(bayu-shell-screenshot STATIC
    src/shell/screenshot/capture_clipboard.cpp
    src/shell/screenshot/capture_clipboard.h
    src/shell/screenshot/capture_notifier.cpp
    src/shell/screenshot/capture_notifier.h
    src/shell/screenshot/capture_overlay.cpp
    src/shell/screenshot/capture_overlay.h
    src/shell/screenshot/capture_settings.cpp
    src/shell/screenshot/capture_settings.h
    src/shell/screenshot/capture_toolbar.cpp
    src/shell/screenshot/capture_toolbar.h
    src/shell/screenshot/screen_capture.cpp
    src/shell/screenshot/screen_capture.h
    src/shell/screenshot/screen_grabber.cpp
    src/shell/screenshot/screen_grabber.h
    src/shell/screenshot/screen_recording.cpp
    src/shell/screenshot/screen_recording.h
)
target_link_libraries(bayu-shell-screenshot PUBLIC bayu-shell bayu-shell-common
    bayu-wayland bayu-wayland-private-shell Bayu::AudioClient)
target_link_libraries(bayu-compositor PRIVATE bayu-shell-screenshot)
# The session's secret agents (NetworkManager, BlueZ) and their prompts.
find_package(BayuClients REQUIRED COMPONENTS networkmanager bluez)
add_library(bayu-shell-secrets STATIC
    src/shell/secrets/bluetooth_pairing_dialog.cpp
    src/shell/secrets/bluetooth_pairing_dialog.h
    src/shell/secrets/network_dialogs.cpp
    src/shell/secrets/network_dialogs.h
    src/shell/secrets/network_secret_agent.cpp
    src/shell/secrets/network_secret_agent.h
    src/shell/secrets/secret_dialog.cpp
    src/shell/secrets/secret_dialog.h
    src/shell/secrets/secret_prompts.cpp
    src/shell/secrets/secret_prompts.h
    src/shell/secrets/secret_store.cpp
    src/shell/secrets/secret_store.h
    src/shell/secrets/vpn_secret_fields.cpp
    src/shell/secrets/vpn_secret_fields.h
)
target_link_libraries(bayu-shell-secrets PUBLIC bayu-shell
    Bayu::NetworkManagerClient Bayu::BluezClient)
target_link_libraries(bayu-compositor PRIVATE bayu-shell-secrets)
# The notification banners and the notification centre on their backend.
add_library(bayu-shell-notification-view STATIC
    src/shell/notification/notification_card.cpp
    src/shell/notification/notification_card.h
    src/shell/notification/notification_centre.cpp
    src/shell/notification/notification_centre.h
    src/shell/notification/notification_controls.cpp
    src/shell/notification/notification_controls.h
    src/shell/notification/notification_motion.cpp
    src/shell/notification/notification_motion.h
    src/shell/notification/notification_popups.cpp
    src/shell/notification/notification_popups.h
    src/shell/notification/notification_ui.cpp
    src/shell/notification/notification_ui.h
)
target_link_libraries(bayu-shell-notification-view PUBLIC bayu-shell bayu-shell-notification)
target_link_libraries(bayu-compositor PRIVATE bayu-shell-notification-view)
# The launcher: its window, grid, search and paging on the launcher's backend.
add_library(bayu-shell-launcher-view STATIC
    src/shell/launcher/launcher_cell.cpp
    src/shell/launcher/launcher_cell.h
    src/shell/launcher/launcher_metrics.cpp
    src/shell/launcher/launcher_metrics.h
    src/shell/launcher/launcher_search_field.cpp
    src/shell/launcher/launcher_search_field.h
    src/shell/launcher/launcher_tween.h
    src/shell/launcher/launcher_view.cpp
    src/shell/launcher/launcher_view.h
    src/shell/launcher/launcher_wallpaper.cpp
    src/shell/launcher/launcher_wallpaper.h
    src/shell/launcher/launcher_window.cpp
    src/shell/launcher/launcher_window.h
)
target_link_libraries(bayu-shell-launcher-view PUBLIC bayu-shell bayu-shell-launcher
    bayu-shell-wallpaper)
target_link_libraries(bayu-compositor PRIVATE bayu-shell-launcher-view)
# The control centre, the status panels and the power dialog.
find_package(BayuClients REQUIRED COMPONENTS networkmanager bluez audio)
add_library(bayu-shell-controlcenter STATIC
    src/shell/controlcenter/control_center.cpp
    src/shell/controlcenter/control_center.h
    src/shell/controlcenter/control_center_services.h
    src/shell/controlcenter/control_center_view.cpp
    src/shell/controlcenter/control_center_view.h
    src/shell/controlcenter/level_slider.cpp
    src/shell/controlcenter/level_slider.h
    src/shell/controlcenter/panel_style.cpp
    src/shell/controlcenter/panel_style.h
    src/shell/controlcenter/power_dialog.cpp
    src/shell/controlcenter/power_dialog.h
    src/shell/controlcenter/session_services.cpp
    src/shell/controlcenter/session_services.h
    src/shell/controlcenter/status_panel.cpp
    src/shell/controlcenter/status_panel.h
    src/shell/controlcenter/tiles.cpp
    src/shell/controlcenter/tiles.h
    src/shell/statusbar/status_bar_panels.h
)
target_link_libraries(bayu-shell-controlcenter PUBLIC bayu-shell bayu-shell-osd
    bayu-shell-statusbar bayu-shell-notification Bayu::NetworkManagerClient
    Bayu::BluezClient Bayu::AudioClient)
target_link_libraries(bayu-compositor PRIVATE bayu-shell-controlcenter)
# The status bar's items, which the lock screen shows too.
add_library(bayu-shell-bar-items STATIC
    src/shell/statusbar/bar_item.cpp
    src/shell/statusbar/bar_item.h
    src/shell/statusbar/status_bar_panels.h
    src/shell/statusbar/status_icon.cpp
    src/shell/statusbar/status_icon.h
)
target_link_libraries(bayu-shell-bar-items PUBLIC bayu-shell bayu-shell-statusbar)
# The status bar: its window, tray, status icons and clock on the bar's backend.
find_package(BayuClients REQUIRED COMPONENTS appearance)
add_library(bayu-shell-statusbar-view STATIC
    src/shell/statusbar/dbus_menu_popup.cpp
    src/shell/statusbar/dbus_menu_popup.h
    src/shell/statusbar/device_usage.cpp
    src/shell/statusbar/device_usage.h
    src/shell/statusbar/panel_tasks.cpp
    src/shell/statusbar/panel_tasks.h
    src/shell/statusbar/pitaloka.h
    src/shell/statusbar/status_bar_backdrop.cpp
    src/shell/statusbar/status_bar_backdrop.h
    src/shell/statusbar/status_bar_menu_bar.h
    src/shell/statusbar/status_bar_panels.h
    src/shell/statusbar/status_bar_window.cpp
    src/shell/statusbar/status_bar_window.h
    src/shell/statusbar/system_tray_view.cpp
    src/shell/statusbar/system_tray_view.h
    src/shell/updates/update_monitor.cpp
    src/shell/updates/update_monitor.h
)
find_package(BayuClients REQUIRED COMPONENTS packagekit)
target_link_libraries(bayu-shell-statusbar-view PUBLIC bayu-shell bayu-shell-bar-items
    bayu-shell-statusbar bayu-shell-wallpaper bayu-shell-dock Bayu::AudioClient Bayu::PackageKitClient)
target_link_libraries(bayu-compositor PRIVATE bayu-shell-statusbar-view)
# The status bar's menu bar.
include(src/shell/menubar/menubar.cmake)
