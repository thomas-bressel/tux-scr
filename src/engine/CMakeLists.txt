# What the plugin does, without Qt nor TuxAPE: plain C++, in four parts,
# tested alone.
#   base     the version, and pictures as the engine has them
#   convert  the conversion: the CPC's screen and palette, the picture,
#            dithering
#   formats  the files written and read: AMSDOS headers, packers, disc
#            images, screens, palettes, assembler sources
#   splits   the split screen and the Plus's table of rasters, and the
#            programs that show them
add_subdirectory(base)
add_subdirectory(convert)
add_subdirectory(formats)
add_subdirectory(splits)
