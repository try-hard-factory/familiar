find_package(QT NAMES Qt6 COMPONENTS Widgets Network REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Network REQUIRED)

##################
#     DOXYGEN    #
##################
# first we can indicate the documentation build as an option and set it to ON by default
option(BUILD_DOC "Build documentation" ON)

# check if Doxygen is installed
find_package(Doxygen)
if (DOXYGEN_FOUND)
    # set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/../docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")

    # note the option ALL which allows to build the docs together with the application
    add_custom_target( doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM )
else (DOXYGEN_FOUND)
  message("Doxygen need to be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
message(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_BINARY_DIR})

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        canvasview.cpp
        canvasview.h
        moveitem.cpp
        moveitem.h
        canvasscene.cpp
        canvasscene.h
        debug_macros.h
        Vec2d.h
        log/log.h
        log/log.cpp
        log/qt_formatters.h
        log/qt_bridge.cpp
        log/ring_sink.h
        log/ring_sink.cpp
        fml_archive.cpp
        fml_archive.h
        recovery.cpp
        recovery.h
        export.cpp
        export.h
        project_settings.cpp
        project_settings.h
        tabpane.cpp
        tabpane.h
        file_actions.cpp
        file_actions.h
        ui/settings_window.cpp
        ui/settings_window.h
        ui/presetsave_window.cpp
        ui/presetsave_window.h
        ui/text_edit_toolbar.cpp
        ui/text_edit_toolbar.h
        ui/gif_playback_toolbar.cpp
        ui/gif_playback_toolbar.h
        ui/group_toolbar.cpp
        ui/group_toolbar.h
        ui/hierarchy_panel.cpp
        ui/hierarchy_panel.h
        actions/actions.cpp
        actions/actions.h
        actions/menu_structure.cpp
        actions/menu_structure.h
        actions/action_mouse_dispatch.cpp
        actions/action_mouse_dispatch.h
        core/qguiappcurrentscreen.cpp
        core/qguiappcurrentscreen.h
        core/controls.cpp
        core/controls.h
        core/held_buttons_tracker.cpp
        core/held_buttons_tracker.h
        core/settings.cpp
        core/settings.h
        core/settingshandler.cpp
        core/settingshandler.h
        core/valuehandler.cpp
        core/valuehandler.h
        ui/colors_widget.h
        ui/colors_widget.cpp
        ui/extendedslider.cpp
        ui/extendedslider.h
        utils/utils.cpp
        utils/utils.h
        selector.cpp
        selector.h
        commands.cpp
        commands.h
        fileio.cpp
        fileio.h
        widgets/main_controls.cpp
        widgets/main_controls.h
        widgets/recent_files_model.cpp
        widgets/recent_files_model.h
        widgets/recent_files_view.cpp
        widgets/recent_files_view.h
        widgets/welcome_overlay.cpp
        widgets/welcome_overlay.h
        widgets/dialogs.cpp
        widgets/dialogs.h
        widgets/message_box.cpp
        widgets/message_box.h
        widgets/dialog_style.cpp
        widgets/dialog_style.h
        widgets/settings_style.cpp
        widgets/settings_style.h
        widgets/flat_checkbox.cpp
        widgets/flat_checkbox.h
        widgets/flat_spinbox.cpp
        widgets/flat_spinbox.h
        widgets/flat_combobox.cpp
        widgets/flat_combobox.h
        widgets/color_picker_dialog.cpp
        widgets/color_picker_dialog.h
        widgets/file_browser_dialog.cpp
        widgets/file_browser_dialog.h
        widgets/save_all_dialog.cpp
        widgets/save_all_dialog.h
        widgets/help_dialog.cpp
        widgets/help_dialog.h
        widgets/about_dialog.cpp
        widgets/about_dialog.h
        widgets/setting_row.cpp
        widgets/setting_row.h
        widgets/setting_descriptions.cpp
        widgets/setting_descriptions.h
        widgets/color_gamut.cpp
        widgets/color_gamut.h
        widgets/controls/controls_common.cpp
        widgets/controls/controls_common.h
        widgets/controls/binding_target.cpp
        widgets/controls/binding_target.h
        widgets/controls/binding_dialogs.cpp
        widgets/controls/binding_dialogs.h
        widgets/controls/bindings_tree_widget.cpp
        widgets/controls/bindings_tree_widget.h
        widgets/controls/keyboard_shortcuts_page.cpp
        widgets/controls/keyboard_shortcuts_page.h
        widgets/controls/search_highlight.h
)
add_executable(familiar ${PROJECT_SOURCES})

if (WIN32)
    set_property(TARGET familiar PROPERTY WIN32_EXECUTABLE true)
endif ()

target_sources(
        familiar
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../data/graphics.qrc
        ${CMAKE_CURRENT_SOURCE_DIR}/../data/familiar.rc # windows binary icon resource file
        main.cpp)

if (MSVC)
    # Our FLOG_* macros add a second layer of variadic-macro forwarding on
    # top of quill's own LOG_* macros (FLOG_DEBUG -> LOG_DEBUG ->
    # QUILL_LOG_DEBUG -> ...). MSVC's legacy (non-conformant) preprocessor
    # mishandles __VA_ARGS__ across multiple macro-to-macro forwards,
    # producing garbled arguments to quill's `static constexpr MacroMetadata`
    # initializer ("cannot convert from initializer list" / "expression did
    # not evaluate to a constant"). The conformant preprocessor (default on
    # GCC/Clang) handles this correctly; MSVC needs it opted in explicitly.
    target_compile_options(familiar PRIVATE /Zc:preprocessor)
endif()

if (WIN32)
    message(!!!!!!!!)
    target_link_libraries(familiar PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        quill::quill
        miniz
    )
elseif(APPLE)

else()
    target_link_libraries(familiar PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        quill::quill
        miniz
    )
endif()



if (WIN32)
    set(USE_OPENSSL FALSE)
    if (ENABLE_OPENSSL)
        find_package(OpenSSL)
        if (OPENSSL_FOUND)
            message(STATUS "OpenSSL support enabled.")
            set(USE_OPENSSL TRUE)
        endif ()
    else ()
        mark_as_advanced(CLEAR OPENSSL_LIBRARIES OPENSSL_INCLUDE_DIR)
    endif ()

    if (NOT USE_OPENSSL)
        message(WARNING "OpenSSL is required to upload screenshots")
    endif ()
endif ()

# ######################################################################################################################
# Installation instructions

include(GNUInstallDirs)
# set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/Flameshot)

set_target_properties(familiar
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS}  -Wall  -O0 -g" # -Wextra -Wpedantic -Werror
)

# Install binary
install(TARGETS familiar
        EXPORT familiar-targets
        BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Desktop entry + .fml file association (see data/img/app/familiar.svg).
# Registers the app and gives .fml files their own icon in the file
# manager once installed - Exec= has a %f placeholder for
# double-click-to-open, but CommandlineArgs never gets a caller for the
# file path it already parses (src/core/settings.h), so that part isn't
# functional yet.
if (UNIX AND NOT APPLE)
    set(FAMILIAR_APP_ID "org.tryhardfactory.Familiar")

    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../data/desktopEntry/package/${FAMILIAR_APP_ID}.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../packaging/linux/${FAMILIAR_APP_ID}.xml
            DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)

    # App icon, in the hicolor theme under the desktop entry's Icon= name.
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../data/img/app/familiar.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
            RENAME ${FAMILIAR_APP_ID}.svg)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../data/img/app/familiar_256.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
            RENAME ${FAMILIAR_APP_ID}.png)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../data/img/app/familiar_512.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
            RENAME ${FAMILIAR_APP_ID}.png)

    # .fml mimetype icon - same mark reused rather than a distinct document icon.
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../data/img/app/familiar.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/mimetypes
            RENAME ${FAMILIAR_APP_ID}.svg)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../data/img/app/familiar_256.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/mimetypes
            RENAME ${FAMILIAR_APP_ID}.png)
endif ()

# macOS bundle Info.plist + icon, including the .fml document type
# (packaging/macos/Info.plist's CFBundleDocumentTypes). Wires up a file
# that used to sit unused - CMAKE_MACOSX_BUNDLE otherwise makes CMake
# generate its own minimal plist instead.
if (APPLE)
    set(FAMILIAR_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/macos/familiar.icns")
    set_target_properties(familiar PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/../packaging/macos/Info.plist
        MACOSX_BUNDLE_ICON_FILE familiar)
    if (EXISTS ${FAMILIAR_ICNS})
        target_sources(familiar PRIVATE ${FAMILIAR_ICNS})
        set_source_files_properties(${FAMILIAR_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    else ()
        message(WARNING "packaging/macos/familiar.icns not found - the app "
                "bundle will use a generic icon until it's added (see "
                "Info.plist's CFBundleIconFile / CFBundleTypeIconFile).")
    endif ()
endif ()

# windeployqt
if (WIN32)
    if (EXISTS $ENV{QTDIR}/bin/windeployqt.exe)
        if (CMAKE_BUILD_TYPE MATCHES Release)
            set(BINARIES_TYPE --release)
        else ()
            set(BINARIES_TYPE --debug)
        endif ()

        add_custom_command(
                TARGET familiar
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/windeployqt_stuff
                COMMAND
                $ENV{QTDIR}/bin/windeployqt.exe ${BINARIES_TYPE} --no-translations --compiler-runtime --no-system-d3d-compiler
                --no-quick-import --dir ${CMAKE_BINARY_DIR}/windeployqt_stuff $<TARGET_FILE:familiar>
                # copy translations manually QM_FILES
                #COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/windeployqt_stuff/translations
                #COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/src/translations
                #${CMAKE_BINARY_DIR}/windeployqt_stuff/translations
                )

        install(DIRECTORY ${CMAKE_BINARY_DIR}/windeployqt_stuff/ DESTINATION bin)


        STRING(REGEX REPLACE "\\\\" "/" OPENSSL_ROOT_DIR $ENV{OPENSSL_ROOT_DIR} )
 
        if (ENABLE_OPENSSL)
            if (EXISTS ${OPENSSL_ROOT_DIR}/bin)

             install(
                        DIRECTORY ${OPENSSL_ROOT_DIR}/bin/
                        DESTINATION bin
                        FILES_MATCHING
                        PATTERN "*.dll")
            else ()
                message(WARNING "Unable to find OpenSSL dlls.")
            endif ()
        endif ()

    else ()
        message("Unable to find executable QTDIR/bin/windeployqt.")
    endif ()
endif ()
