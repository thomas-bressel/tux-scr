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

# TuxAPE's sources: all the plugin needs of them is src/app/plugin.h.
set(TUXAPE_DIR "${CMAKE_SOURCE_DIR}/../TuxAPE" CACHE PATH "TuxAPE's source tree")
if(NOT EXISTS "${TUXAPE_DIR}/src/app/plugin.h")
    message(FATAL_ERROR "TuxAPE's plugin.h is not in ${TUXAPE_DIR}/src/app: set TUXAPE_DIR to TuxAPE's source tree.")
endif()

option(TUXSCR_BUILD_TESTS "Build the test programs" ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# What the machine is called in a plugin's plugin.json: as TuxAPE's
# PluginManager::platform() gives it.
string(TOLOWER "${CMAKE_SYSTEM_NAME}" TUXSCR_SYSTEM)
if(TUXSCR_SYSTEM STREQUAL "darwin")
    set(TUXSCR_SYSTEM "macos")
endif()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" TUXSCR_PROCESSOR)
if(TUXSCR_PROCESSOR STREQUAL "amd64")
    set(TUXSCR_PROCESSOR "x86_64")
elseif(TUXSCR_PROCESSOR STREQUAL "aarch64")
    set(TUXSCR_PROCESSOR "arm64")
endif()
set(TUXSCR_PLATFORM "${TUXSCR_SYSTEM}-${TUXSCR_PROCESSOR}")

# The conversion and the files it writes have no dependency on Qt: they are
# tested by themselves.
add_subdirectory(src/base)
add_subdirectory(src/engine)
add_subdirectory(src/formats)
add_subdirectory(src/plugin)
add_subdirectory(src/sprites)
add_subdirectory(src/splits)
add_subdirectory(src/module)
add_subdirectory(src/cli)

if(TUXSCR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- The archive users install: dist/tuxscr-<version>-<platform>.zip ----
# It holds the plugin's folder, as TuxAPE's plugins folder does.
set(TUXSCR_STAGE "${CMAKE_BINARY_DIR}/stage")
set(TUXSCR_ARCHIVE "${CMAKE_SOURCE_DIR}/dist/tuxscr-${PROJECT_VERSION}-${TUXSCR_PLATFORM}.zip")
configure_file(plugin.json.in "${TUXSCR_STAGE}/tuxscr/plugin.json" @ONLY)
# The command-line program goes with it, in the folder's bin: TuxAPE makes
# what it unpacks there a program that can be run.
add_custom_target(plugin_zip ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${TUXSCR_STAGE}/tuxscr/${TUXSCR_PLATFORM}" "${TUXSCR_STAGE}/tuxscr/bin"
            "${CMAKE_SOURCE_DIR}/dist"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:tuxscr>" "${TUXSCR_STAGE}/tuxscr/${TUXSCR_PLATFORM}/"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:tuxscr_cli>" "${TUXSCR_STAGE}/tuxscr/bin/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/LICENSE" "${TUXSCR_STAGE}/tuxscr/"
    COMMAND ${CMAKE_COMMAND} -E chdir "${TUXSCR_STAGE}"
            ${CMAKE_COMMAND} -E tar cf "${TUXSCR_ARCHIVE}" --format=zip tuxscr
    DEPENDS tuxscr tuxscr_cli
    COMMENT "Packing ${TUXSCR_ARCHIVE}"
    VERBATIM)
