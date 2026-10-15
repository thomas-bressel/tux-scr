find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

# What the converter's window is given of the Plus's hardware sprites: their editor, their capture from the picture, their places on it.
add_library(tuxscr_sprites STATIC
    # What the window is given, and the texts of it all.
    spritetools.cpp
    spritetools.h
    spritetexts.cpp
    spritetexts.h
    # The sprite editor, and the windows it asks things in.
    spriteeditor.cpp
    spriteeditor.h
    spritepalettedialog.cpp
    spritepalettedialog.h
    spritesavedialog.cpp
    spritesavedialog.h
    spriteicons.cpp
    spriteicons.h
    # Under the picture: sprites captured from it, and shown on it.
    spritecapture.cpp
    spritecapture.h
    spriteplaces.cpp
    spriteplaces.h
    # What they draw themselves, and the files they write and read.
    spriteviews.cpp
    spriteviews.h
    spritefiles.cpp
    spritefiles.h
)
target_include_directories(tuxscr_sprites PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tuxscr_sprites PUBLIC tuxscr_ui)
