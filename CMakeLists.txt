cmake_minimum_required(VERSION 3.5)

set(FAMILIAR_VERSION 0.0.6)

set(GIT_API_URL "https://api.github.com/repos/try-hard-factory/familiar/releases/latest")

project(familiar VERSION ${FAMILIAR_VERSION} LANGUAGES CXX)

set(PROJECT_NAME_CAPITALIZED "Familiar")

if (APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
endif()

# Configuration options
set(DEFAULT_RUN_IN_PLACE FALSE)
if(WIN32)
  set(DEFAULT_RUN_IN_PLACE TRUE)
  # For Windows RC file.
  add_definitions(-DFAMILIAR_VERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR})
  add_definitions(-DFAMILIAR_VERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR})
  add_definitions(-DFAMILIAR_VERSION_BUGFIX=${CMAKE_PROJECT_VERSION_PATCH})
  add_definitions(-DFAMILIAR_VERSION_BUILD=1)
  add_definitions(-DFAMILIAR_VERSION_STRING="${PROJECT_VERSION}")
elseif(APPLE)
  set(Qt6_DIR "$(brew --prefix qt6)/lib/cmake/Qt6/" CACHE PATH "directory where Qt6Config.cmake exists.")
  set(CMAKE_MACOSX_BUNDLE ON)
  set(CMAKE_MACOSX_RPATH ON)
endif()
set(RUN_IN_PLACE
        ${DEFAULT_RUN_IN_PLACE}
        CACHE BOOL "Run directly in source directory structure")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
option(GENERATE_TS "Regenerate translation source files" OFF)

include(cmake/StandardProjectSettings.cmake)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_11)

add_library(project_warnings INTERFACE)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_subdirectory(src)

# CPack
set(CPACK_PACKAGE_VENDOR "try-hard-factory")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "For 2d and 3d artists.")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_HOMEPAGE_URL "https://tryhardfactory.org")
set(CPACK_PACKAGE_CONTACT "max <mpano91@gmail.com>")
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/data/img/app/org.flameshot.Flameshot.svg") # TODO: Can any generator make
# use of this?
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md") # TODO: Where is this used? Do we need a better
# source?

if(WIN32)
  # Include all dynamically linked runtime libraries such as MSVCRxxx.dll
  include(InstallRequiredSystemLibraries)

  if(RUN_IN_PLACE)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-win64")
    else()
      set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-win32")
    endif()

    set(CPACK_GENERATOR ZIP)

  else()
    set(CPACK_GENERATOR WIX ZIP)
    set(CPACK_PACKAGE_NAME "${PROJECT_NAME_CAPITALIZED}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME_CAPITALIZED}")
    set(CPACK_PACKAGE_EXECUTABLES ${PROJECT_NAME} "${PROJECT_NAME_CAPITALIZED}")
    set(CPACK_CREATE_DESKTOP_LINKS ${PROJECT_NAME})

    # WIX (Windows .msi installer)
    # 48x48 pixels
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/data/img/app/flameshot.ico")
    # Supported languages can be found at http://wixtoolset.org/documentation/manual/v3/wixui/wixui_localization.html
    # set(CPACK_WIX_CULTURES "ar-SA,bg-BG,ca-ES,hr-HR,cs-CZ,da-DK,nl-NL,en-US,et-EE,fi-FI,fr-FR,de-DE")
    set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/packaging/win-installer/Bitmaps/CPACK_WIX_UI_BANNER.BMP")
    set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/packaging/win-installer/Bitmaps/CPACK_WIX_UI_DIALOG.BMP")
    set(CPACK_WIX_PROPERTY_ARPHELPLINK "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_WIX_ROOT_FEATURE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")
    set(CPACK_WIX_LIGHT_EXTRA_FLAGS "-dcl:high") # set high compression

    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/packaging/win-installer/LICENSE/GPL-3.0.txt")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

    # The correct way would be to include both x32 and x64 into one installer and install the appropriate one. CMake
    # does not support that, so there are two separate GUID's
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(CPACK_WIX_UPGRADE_GUID "7fe72477-fd44-4c77-849d-9dd597fb5b54")
    else()
      set(CPACK_WIX_UPGRADE_GUID "6b532710-6461-49d0-9a26-c0d576ff018d")
    endif()
  endif()
elseif(APPLE)
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
  set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-osx")
  set(CPACK_GENERATOR ZIP)
else()
  set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-linux")
  set(CPACK_GENERATOR TGZ)
  set(CPACK_SOURCE_GENERATOR TGZ)
endif()

include(CPack)
