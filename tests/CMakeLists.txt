find_package(Qt6 REQUIRED COMPONENTS Gui Test Widgets)

# The tests of the engine: every engine_*.cpp of this folder is a program of
# its own, linked with the engine and nothing else.
file(GLOB TUXSCR_ENGINE_TESTS CONFIGURE_DEPENDS engine_*.cpp)
foreach(source ${TUXSCR_ENGINE_TESTS})
    get_filename_component(test ${source} NAME_WE)
    add_executable(${test} ${source})
    target_link_libraries(${test} PRIVATE tuxscr_splits tuxscr_formats)
    add_test(NAME ${test} COMMAND ${test})
endforeach()

# The plugin as TuxAPE loads it: found as a library, started with a host of
# the test's own, its window shown; in English, then in French.
add_executable(plugin_window plugin_window.cpp)
target_include_directories(plugin_window PRIVATE ${TUXAPE_SDK})
target_link_libraries(plugin_window PRIVATE Qt6::Widgets Qt6::Test)
add_dependencies(plugin_window tuxscr)
target_compile_definitions(plugin_window PRIVATE TUXSCR_PLUGIN="$<TARGET_FILE:tuxscr>")
add_test(NAME plugin_window COMMAND plugin_window)
set_tests_properties(plugin_window PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# The windows, driven as a user drives them; in French too, in windows
# made while the plugin's translation is installed (french.h).
file(GLOB TUXSCR_GUI_TESTS CONFIGURE_DEPENDS gui_*.cpp)
foreach(source ${TUXSCR_GUI_TESTS})
    get_filename_component(test ${source} NAME_WE)
    add_executable(${test} ${source})
    target_link_libraries(${test} PRIVATE tuxscr_ui Qt6::Test)
    # TuxSCR's French, which the plugin's build makes for its library.
    target_compile_definitions(${test} PRIVATE TUXSCR_FRENCH="${CMAKE_BINARY_DIR}/tuxscr_fr.qm")
    add_dependencies(${test} tuxscr)
    add_test(NAME ${test} COMMAND ${test})
    set_tests_properties(${test} PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endforeach()

# The command-line program, run as a user runs it.
add_executable(cli_convert cli_convert.cpp)
target_link_libraries(cli_convert PRIVATE tuxscr_formats Qt6::Gui)
target_compile_definitions(cli_convert PRIVATE TUXSCR_PROGRAM="$<TARGET_FILE:tuxscr_cli>")
add_dependencies(cli_convert tuxscr_cli)
add_test(NAME cli_convert COMMAND cli_convert)
set_tests_properties(cli_convert PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
