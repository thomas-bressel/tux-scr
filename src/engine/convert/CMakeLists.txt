# The conversion: the CPC's screen and palette, the picture, dithering.
add_library(tuxscr_convert STATIC
    screen.cpp
    screen.h
    param.h
    picture.cpp
    picture.h
    dither.cpp
    dither.h
    convert.cpp
    convert.h
)
target_link_libraries(tuxscr_convert PUBLIC tuxscr_base)
