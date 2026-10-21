# The split screen and the Plus's table of rasters, without their windows:
# their lines, the colours they give, their files and the programs that
# show them.
add_library(tuxscr_splits STATIC
    splitscreen.cpp
    splitscreen.h
    splitasm.cpp
    splitasm.h
    rasters.cpp
    rasters.h
    xmltext.h
)
target_link_libraries(tuxscr_splits PUBLIC tuxscr_formats tuxscr_convert tuxscr_base)
