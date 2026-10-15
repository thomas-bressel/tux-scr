find_package(Qt6 REQUIRED COMPONENTS Gui Test Widgets)

# The tests of the engine's parts: every engine_*.cpp and formats_*.cpp of
# this folder is a program of its own, linked with its library.
foreach(part engine formats)
    file(GLOB TUXSCR_PART_TESTS CONFIGURE_DEPENDS ${part}_*.cpp)
    foreach(source ${TUXSCR_PART_TESTS})
        get_filename_component(test ${source} NAME_WE)
        add_executable(${test} ${source})
        target_link_libraries(${test} PRIVATE tuxscr_formats)
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
endforeach()

# The plugin as TuxAPE loads it: found as a library, started with a host of
# the test's own, its window shown.
add_executable(plugin_window plugin_window.cpp)
target_include_directories(plugin_window PRIVATE ${TUXAPE_DIR}/src/app)
target_link_libraries(plugin_window PRIVATE Qt6::Widgets Qt6::Test)
add_dependencies(plugin_window tuxscr)
target_compile_definitions(plugin_window PRIVATE TUXSCR_PLUGIN="$<TARGET_FILE:tuxscr>")
add_test(NAME plugin_window COMMAND plugin_window)
set_tests_properties(plugin_window PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# The windows, driven as a user drives them.
file(GLOB TUXSCR_GUI_TESTS CONFIGURE_DEPENDS gui_*.cpp)
foreach(source ${TUXSCR_GUI_TESTS})
    get_filename_component(test ${source} NAME_WE)
    add_executable(${test} ${source})
    # The tests of the sprites' windows and of the splits' are linked with
    # theirs.
    if(test MATCHES "^gui_sprite")
        target_link_libraries(${test} PRIVATE tuxscr_sprites Qt6::Test)
    elseif(test MATCHES "^gui_(split|raster|genpal)")
        target_link_libraries(${test} PRIVATE tuxscr_splits Qt6::Test)
    else()
        target_link_libraries(${test} PRIVATE tuxscr_ui Qt6::Test)
    endif()
    add_test(NAME ${test} COMMAND ${test})
    set_tests_properties(${test} PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endforeach()

# The palette interrupts run on TuxAPE's own Plus, if TuxAPE is built: the
# source TuxSCR writes is assembled and run by TuxAPE's core.
set(TUXAPE_CORE_LIBRARY "${TUXAPE_DIR}/build/src/core/libtuxape_core.a")
if(EXISTS "${TUXAPE_CORE_LIBRARY}")
    add_executable(tuxape_interrupts tuxape_interrupts.cpp)
    target_include_directories(tuxape_interrupts PRIVATE ${TUXAPE_DIR}/src)
    target_link_libraries(tuxape_interrupts PRIVATE tuxscr_formats "${TUXAPE_CORE_LIBRARY}")
    add_test(NAME tuxape_interrupts COMMAND tuxape_interrupts)
else()
    message(STATUS "TuxAPE's core is not built in ${TUXAPE_DIR}/build: the palette interrupts are not run on it.")
endif()

# The command-line program, run as a user runs it.
add_executable(cli_convert cli_convert.cpp)
target_link_libraries(cli_convert PRIVATE tuxscr_formats Qt6::Gui)
target_compile_definitions(cli_convert PRIVATE TUXSCR_PROGRAM="$<TARGET_FILE:tuxscr_cli>")
add_dependencies(cli_convert tuxscr_cli)
add_test(NAME cli_convert COMMAND cli_convert)
set_tests_properties(cli_convert PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
