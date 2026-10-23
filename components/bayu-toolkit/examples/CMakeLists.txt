add_executable(bayu-toolkit-demo
    demo/main.cpp
)

target_link_libraries(bayu-toolkit-demo PRIVATE bayu-toolkit)

add_executable(bayu-toolkit-gallery
    gallery/main.cpp
)

target_link_libraries(bayu-toolkit-gallery PRIVATE bayu-toolkit)
