# Test source list, kept next to the tests themselves instead of buried
# in src/CMakeLists.txt - included from there via:
#   include(${PROJECT_SOURCE_DIR}/tests/FamiliarTestsEmbed.cmake)
#   target_sources(familiar PRIVATE ${FamiliarTestsSrc})
#
# CMAKE_CURRENT_LIST_DIR (this file's own directory), not
# CMAKE_CURRENT_SOURCE_DIR (the includER's directory) - keeps the paths
# correct regardless of which CMakeLists.txt does the include().

list(APPEND FamiliarTestsSrc
    ${CMAKE_CURRENT_LIST_DIR}/support/settings_test_environment.h
    ${CMAKE_CURRENT_LIST_DIR}/core/commandline_args_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/core/famsettings_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/core/held_buttons_tracker_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/core/controls_test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/utils/utils_test.cpp
)
