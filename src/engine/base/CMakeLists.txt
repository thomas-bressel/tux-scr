# What the rest is built on: the version, and pictures.
add_library(tuxscr_base STATIC
    version.cpp
    version.h
    bitmap.h
)
target_include_directories(tuxscr_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(tuxscr_base PRIVATE TUXSCR_VERSION="${PROJECT_VERSION}")
