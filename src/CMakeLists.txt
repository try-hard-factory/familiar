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

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        canvasview.cpp
        canvasview.h
        moveitem.cpp
        moveitem.h
        movepixmapitem.cpp
        movepixmapitem.h
        canvasscene.cpp
        canvasscene.h
        itemgroup.cpp
        itemgroup.h
        mainselectedarea.cpp
        mainselectedarea.h
        debug_macros.h
        Logger.h
        MessageBuilder.h
        borderdot.cpp
        borderdot.h
        Vec2d.h
        image_downloader.cpp
        image_downloader.h
        fml_file_buffer.cpp
        fml_file_buffer.h
        project_settings.cpp
        project_settings.h
        tabpane.cpp
        tabpane.h
        file_actions.cpp
        file_actions.h
        saveallwindow.cpp
        saveallwindow.h
        savecheckbox.cpp
        savecheckbox.h
        settings_window.cpp
        settings_window.h
        main_context_menu.cpp
        main_context_menu.h
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

target_link_libraries(familiar PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)

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

# include(GNUInstallDirs)
# set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/Flameshot)

# Install binary
install(TARGETS familiar
        EXPORT familiar-targets
        BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

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
