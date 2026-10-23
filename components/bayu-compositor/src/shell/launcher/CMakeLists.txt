# The launcher without its view: its applications in the user's order, the
# search, the pages, and org.bayu.Launcher.
add_library(bayu-shell-launcher STATIC
    launcher.cpp
    launcher.h
    launcher_model.cpp
    launcher_model.h
)
target_link_libraries(bayu-shell-launcher PUBLIC bayu-shell-dock)
