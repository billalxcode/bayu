# The status bar's global menu bar: the user's menu, the active application's
# name and its exported menus, and the desktop's menus.
find_package(BayuClients REQUIRED COMPONENTS accounts)

add_library(bayu-shell-menubar STATIC
    src/shell/menubar/menu_bar_model.cpp
    src/shell/menubar/menu_bar_model.h
    src/shell/menubar/menu_bar_view.cpp
    src/shell/menubar/menu_bar_view.h
)
target_link_libraries(bayu-shell-menubar PUBLIC bayu-shell bayu-shell-statusbar
    bayu-shell-statusbar-view Bayu::AccountsClient)
target_link_libraries(bayu-compositor PRIVATE bayu-shell-menubar)
