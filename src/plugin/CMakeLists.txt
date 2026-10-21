set(CMAKE_AUTOMOC ON)

# The windows are a library, so that tests can drive them; the plugin itself
# is no more than what TuxAPE loads and talks to.
add_library(tuxscr_ui STATIC
    # The converter's window: settings on the left, the picture on the right.
    converterwindow.cpp
    converterwindow.h
    palettebar.cpp
    palettebar.h
    pictureview.cpp
    pictureview.h
    interruptspanel.cpp
    interruptspanel.h
    colourdialog.cpp
    colourdialog.h
    # The picture's editor, and the grids over the picture.
    edittools.cpp
    edittools.h
    # The frames of an animation, the patterns of mode ASC-UT, and the
    # settings ConvImgCpc calls internal.
    animationpanel.cpp
    animationpanel.h
    patterndialog.cpp
    patterndialog.h
    internalsettings.cpp
    internalsettings.h
    # What the windows work on together.
    document.cpp
    document.h
    # What they share: the texts of ConvImgCpc's windows (windowtexts.h is
    # made by tools/make_tables.py) and TuxSCR's own.
    windowtexts.h
    texts.cpp
    texts.h
    penbutton.cpp
    penbutton.h
    prompts.cpp
    prompts.h
    contrast.h
    controls.h
    icons.cpp
    icons.h
    # The split editor, the Plus's table of rasters and the maker of
    # palettes: what the converter's window is given of them.
    splits/splittools.cpp
    splits/splittools.h
    splits/spliteditor.cpp
    splits/spliteditor.h
    splits/rastertable.cpp
    splits/rastertable.h
    splits/palettemaker.cpp
    splits/palettemaker.h
    splits/shadecontrols.cpp
    splits/shadecontrols.h
    splits/swatch.cpp
    splits/swatch.h
    splits/splittexts.cpp
    splits/splittexts.h
    # The Plus's hardware sprites: their editor, their capture from the
    # picture, their places on it.
    sprites/spritetools.cpp
    sprites/spritetools.h
    sprites/spritetexts.cpp
    sprites/spritetexts.h
    sprites/spriteeditor.cpp
    sprites/spriteeditor.h
    sprites/spritepalettedialog.cpp
    sprites/spritepalettedialog.h
    sprites/spritesavedialog.cpp
    sprites/spritesavedialog.h
    sprites/spriteicons.cpp
    sprites/spriteicons.h
    sprites/spritecapture.cpp
    sprites/spritecapture.h
    sprites/spriteplaces.cpp
    sprites/spriteplaces.h
    sprites/spriteviews.cpp
    sprites/spriteviews.h
    sprites/spritefiles.cpp
    sprites/spritefiles.h
)
target_include_directories(tuxscr_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/splits
                                            ${CMAKE_CURRENT_SOURCE_DIR}/sprites ${TUXAPE_SDK})
target_link_libraries(tuxscr_ui PUBLIC tuxscr_io tuxscr_splits tuxscr_formats tuxscr_convert tuxscr_base Qt6::Widgets)

# What TuxAPE loads: the plugin's class, and no more. tuxape_add_plugin()
# names it after the plugin's id.
add_library(tuxscr MODULE
    tuxscrplugin.cpp
    tuxscrplugin.h
)
target_link_libraries(tuxscr PRIVATE tuxscr_ui)
