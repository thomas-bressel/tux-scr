find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

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
    # What they share.
    penbutton.cpp
    penbutton.h
    pictures.cpp
    pictures.h
    prompts.cpp
    prompts.h
    texts.cpp
    texts.h
    contrast.h
    controls.h
    icons.cpp
    icons.h
)
target_include_directories(tuxscr_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${TUXAPE_DIR}/src/app)
target_link_libraries(tuxscr_ui PUBLIC tuxscr_formats tuxscr_engine tuxscr_base Qt6::Widgets)
