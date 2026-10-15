find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

# What TuxAPE loads: no more than what it talks to, the windows being
# libraries that tests drive by themselves.
add_library(tuxscr MODULE
    tuxscrplugin.cpp
    tuxscrplugin.h
)
target_link_libraries(tuxscr PRIVATE tuxscr_sprites tuxscr_splits tuxscr_ui)
