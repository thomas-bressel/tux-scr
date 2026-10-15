# The command-line program: ConvImgCpc's command line. It reads pictures
# with Qt, as the plugin does.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui)

# The program is called tuxscr; its target cannot be, the plugin's is.
add_executable(tuxscr_cli main.cpp)
set_target_properties(tuxscr_cli PROPERTIES OUTPUT_NAME tuxscr)
target_link_libraries(tuxscr_cli PRIVATE tuxscr_ui Qt6::Core Qt6::Gui)
