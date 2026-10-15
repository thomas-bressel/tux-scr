# The files written and read: AMSDOS headers, packers, disc images, screens,
# palettes, assembler sources.
add_library(tuxscr_formats STATIC
    amsdos.cpp
    amsdos.h
    pack.cpp
    pack.h
    dsk.cpp
    dsk.h
    asmtext.h
    asmgen.cpp
    asmgen.h
    screenfile.cpp
    screenfile.h
    paramfile.cpp
    paramfile.h
    animfile.cpp
    animfile.h
)
target_link_libraries(tuxscr_formats PUBLIC tuxscr_engine tuxscr_base)
