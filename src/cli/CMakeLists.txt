# The command-line program: ConvImgCpc's command line. It is made of the
# engine, and reads the PC's pictures with Qt's, as the windows do; it has
# no window of its own.
#
# The program is called tuxscr; its target cannot be, the plugin's is.
add_executable(tuxscr_cli main.cpp)
set_target_properties(tuxscr_cli PROPERTIES OUTPUT_NAME tuxscr)
target_link_libraries(tuxscr_cli PRIVATE tuxscr_pictures tuxscr_formats tuxscr_convert tuxscr_base Qt6::Gui)
