# TuxSCR: ConvImgCpc's screen converter for the Amstrad CPC and Plus, as a
# TuxAPE plugin
# (see PLUGINS.md at TuxAPE's root).
cmake_minimum_required(VERSION 3.21)

project(TuxSCR
    VERSION 0.1.0
    DESCRIPTION "Screen converter for the Amstrad CPC and Plus: a plugin for TuxAPE, after ConvImgCpc"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# The engine goes into the plugin, a shared library.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Qt, for the whole project: TuxAPE's, or an earlier Qt 6 (checked by
# tuxape_add_plugin).
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

# TuxAPE's plugin SDK: plugin.h and TuxAPEPlugin.cmake, all the plugin
# needs of TuxAPE.
set(TUXAPE_SDK "${CMAKE_SOURCE_DIR}/../TuxAPE/sdk" CACHE PATH "TuxAPE's sdk folder")
if(NOT EXISTS "${TUXAPE_SDK}/plugin.h" OR NOT EXISTS "${TUXAPE_SDK}/TuxAPEPlugin.cmake")
    message(FATAL_ERROR "TuxAPE's SDK is not in ${TUXAPE_SDK}: set TUXAPE_SDK to TuxAPE's sdk folder.")
endif()
include("${TUXAPE_SDK}/TuxAPEPlugin.cmake")

option(TUXSCR_BUILD_TESTS "Build the test programs" ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The conversion and the files it writes, without Qt: tested by themselves,
# and what the command-line program is made of.
add_subdirectory(src/engine)
# What TuxAPE loads: the windows, and the plugin's class.
add_subdirectory(src/plugin)
# tuxscr: ConvImgCpc's command line.
add_subdirectory(src/cli)

# The archive users install: dist/tuxscr-<version>-<platform>.zip, with the
# command-line program in its bin folder and the plugin's translations
# built into its library; signed when the author's secret key is named
# (TUXAPE_SIGN_KEY).
tuxape_add_plugin(tuxscr
    ID tuxscr
    NAME "TuxSCR – Screen Converter"
    VERSION ${PROJECT_VERSION}
    AUTHOR "TuxAPE"
    LICENCE "see LICENSE"
    DESCRIPTION "Converts pictures into Amstrad CPC and Plus screens (modes 0 to 2, overscan, EGX, mode X, split rasters, ASCII), as ConvImgCpc by Ludovic Deplanque (Demoniak) does, edits them, and writes them as files, assembler sources or disc images. On a Plus, changes the palette at chosen lines with interrupts."
    API 4
    FILES LICENSE
    PROGRAMS tuxscr_cli
    TRANSLATIONS translations/tuxscr_fr.ts
    TRANSLATE_TARGETS tuxscr_ui)

if(TUXSCR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
