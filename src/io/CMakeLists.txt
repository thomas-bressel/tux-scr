# Pictures between Qt and the engine: a file's pictures read, and one
# brought to the size of the CPC's screen. Qt's pictures only, no window:
# the command-line program reads its pictures with it too.
add_library(tuxscr_io STATIC
    pictures.cpp
    pictures.h
)
target_include_directories(tuxscr_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tuxscr_io PUBLIC tuxscr_convert tuxscr_base Qt6::Gui)
