find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

# What the converter's window is given of split rasters, the Plus's table of rasters and the maker of palettes.
add_library(tuxscr_splits STATIC
    splittools.cpp
    splittools.h
    # The split screen and the raster table, without their windows.
    splitscreen.cpp
    splitscreen.h
    splitasm.cpp
    splitasm.h
    rasters.cpp
    rasters.h
    xmltext.h
    # Their windows.
    spliteditor.cpp
    spliteditor.h
    rastertable.cpp
    rastertable.h
    palettemaker.cpp
    palettemaker.h
    # What the windows share.
    shadecontrols.cpp
    shadecontrols.h
    swatch.cpp
    swatch.h
    splittexts.cpp
    splittexts.h
)
target_include_directories(tuxscr_splits PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tuxscr_splits PUBLIC tuxscr_ui)
