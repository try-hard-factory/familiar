# Test source list, kept next to the tests themselves instead of buried
# in src/CMakeLists.txt - included from there via:
#   include(${PROJECT_SOURCE_DIR}/tests/FamiliarTestsEmbed.cmake)
#   target_sources(familiar PRIVATE ${FamiliarTestsSrc})
#
# CMAKE_CURRENT_LIST_DIR (this file's own directory), not
# CMAKE_CURRENT_SOURCE_DIR (the includER's directory) - keeps the paths
# correct regardless of which CMakeLists.txt does the include().

list(APPEND FamiliarTestsSrc
    ${CMAKE_CURRENT_LIST_DIR}/support/log_test_environment.h
    ${CMAKE_CURRENT_LIST_DIR}/support/settings_test_environment.h
    ${CMAKE_CURRENT_LIST_DIR}/core/commandline_args_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/core/famsettings_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/core/held_buttons_tracker_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/core/controls_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/core/keyboard_settings_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/core/settingshandler_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/core/valuehandler_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/log/log_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/utils/utils_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/flat_checkbox_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/flat_spinbox_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/flat_combobox_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/setting_row_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/setting_descriptions_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/dialog_style_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/settings_style_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/binding_dialogs_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/binding_editor_dialog_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/keyboard_shortcuts_page_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/color_picker_dialog_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/message_box_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/actions/menu_structure_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/search_highlight_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/binding_target_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/widgets/bindings_tree_widget_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/actions/action_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/actions/action_registry_test.cpp
)
