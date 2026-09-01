cmake_minimum_required(VERSION 3.5)

set(FAMILIAR_VERSION 0.0.16)

set(GIT_API_URL "https://api.github.com/repos/try-hard-factory/familiar/releases/latest")

project(familiar VERSION ${FAMILIAR_VERSION} LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROJECT_NAME_CAPITALIZED "Familiar")

if (NOT CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DNDEBUG)
endif()

# appVersion in the .fml manifest (docs/fml_format_design.md §5) -
# platform-independent, unlike the WIN32-only FAMILIAR_VERSION_* block below.
add_definitions(-DFAMILIAR_VERSION_STRING="${PROJECT_VERSION}")

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
elseif(APPLE)
  set(Qt6_DIR "$(brew --prefix qt6)/lib/cmake/Qt6/" CACHE PATH "directory where Qt6Config.cmake exists.")
  set(CMAKE_MACOSX_BUNDLE ON)
  set(CMAKE_MACOSX_RPATH ON)
endif()
set(RUN_IN_PLACE
        ${DEFAULT_RUN_IN_PLACE}
        CACHE BOOL "Run directly in source directory structure")
# Lets C++ branch on this too (utils::portableDataDir()) - CMake-side
# RUN_IN_PLACE only ever controlled CPack's packaging/install layout
# (ZIP vs WIX below) until now; settings.json/recovery/the log file all
# still wrote to the per-user AppData/.config location even in a
# "portable" build, which isn't actually portable at all.
if(RUN_IN_PLACE)
  add_definitions(-DRUN_IN_PLACE)
endif()

#set(CMAKE_INCLUDE_CURRENT_DIR ON)
option(GENERATE_TS "Regenerate translation source files" OFF)

include(cmake/StandardProjectSettings.cmake)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_11)

add_library(project_warnings INTERFACE)


set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GoogleTest - linked into `familiar` unconditionally (see
# src/CMakeLists.txt + main.cpp's "-t" handling, which runs the suite in
# place of the normal GUI startup), not gated behind a build option -
# every build mode gets it. After CMAKE_CXX_STANDARD above, not before -
# so googletest's own CMakeLists.txt (add_subdirectory()'d synchronously
# by FetchContent_MakeAvailable() below) configures at the same C++
# standard as the rest of this project instead of whatever the
# compiler's bare default happens to be.
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.18.0
    # GIT_SHALLOW: single commit at the tag instead of full history -
    # much less to transfer. GIT_PROGRESS: without it, ExternalProject's
    # git clone step logs nothing at all until it finishes, so a slow
    # link makes the build look hung with no way to tell it apart from
    # an actually-stuck clone (confirmed hitting exactly this - see
    # ExternalProject.cmake's own GIT_PROGRESS doc for the same words).
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
# Matches whatever CRT familiar itself links against on Windows; a
# no-op on Linux/macOS. Set before FetchContent_MakeAvailable() so it
# takes before googletest's own CMakeLists.txt reads it.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# googletest's own CMakeLists.txt defaults INSTALL_GTEST to ON, adding
# install(TARGETS gtest/gmock ...) rules tagged COMPONENT "gtest"/"gmock"
# (see its cmake/internal_utils.cmake install_project()) - harmless on
# its own, but CPack sweeps up every install() rule in the whole build
# tree, and those two extra components change how the WiX generator
# assigns component IDs to everything else, including familiar.exe
# itself. Broke Windows packaging with "CPack Error: Some XML patch
# fragments did not have matching IDs: 'CM_CP_bin.familiar.exe'"
# (fml-file-association.xml, CPACK_WIX_PATCH_FILE below, patches that
# exact ID in). gtest/gmock are statically linked straight into familiar
# itself (see main.cpp's "-t" handling) - nothing outside this build ever
# needs them installed separately.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_subdirectory(${CMAKE_SOURCE_DIR}/include/quill)

# miniz: zip read/write for the .fml project file format (see
# docs/fml_format_design.md). Standalone-project defaults (examples,
# fuzzers, tests, install rules) are off automatically once PROJECT_NAME
# is already defined by us, but set them explicitly so a future miniz
# default change can't silently pull them back in.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/include/miniz)

# LibRaw: decodes camera RAW files (NEF/CR3/ARW/...) for the RAW-import
# feature. No CMake build ships upstream - README.cmake says so
# explicitly ("cmake builds are not officially supported by LibRaw team
# since October 23, 2014", pointing at a separate, unofficial
# LibRaw-cmake repo instead) - so this is a plain add_library() here,
# not add_subdirectory() like quill/miniz above. The source list below
# is copied verbatim from LibRaw's own Makefile.am
# (lib_libraw_a_SOURCES) - its real, authoritative "what a standard
# build actually needs" list, not guessed from the directory tree.
# JPEG/zlib support deliberately left off (configure.ac's --enable-jpeg/
# --enable-zlib, both opt-in upstream too) - only needed for lossy-
# compressed/deflate-compressed DNG specifically, not for any of the
# formats familiar actually targets (NEF/CR2/CR3/ARW/ORF/RAF/RW2/PEF/
# SRW/standard DNG).
set(LIBRAW_DIR ${CMAKE_SOURCE_DIR}/include/libraw)
add_library(libraw STATIC
    ${LIBRAW_DIR}/src/libraw_c_api.cpp
    ${LIBRAW_DIR}/src/libraw_datastream.cpp
    ${LIBRAW_DIR}/src/decoders/canon_600.cpp
    ${LIBRAW_DIR}/src/decoders/crx.cpp
    ${LIBRAW_DIR}/src/decoders/pana8.cpp
    ${LIBRAW_DIR}/src/decoders/decoders_dcraw.cpp
    ${LIBRAW_DIR}/src/decoders/sonycc.cpp
    ${LIBRAW_DIR}/src/decompressors/losslessjpeg.cpp
    ${LIBRAW_DIR}/src/decoders/decoders_libraw_dcrdefs.cpp
    ${LIBRAW_DIR}/src/decoders/olympus14.cpp
    ${LIBRAW_DIR}/src/decoders/decoders_libraw.cpp
    ${LIBRAW_DIR}/src/decoders/dng.cpp
    ${LIBRAW_DIR}/src/decoders/fp_dng.cpp
    ${LIBRAW_DIR}/src/decoders/fuji_compressed.cpp
    ${LIBRAW_DIR}/src/decoders/generic.cpp
    ${LIBRAW_DIR}/src/decoders/kodak_decoders.cpp
    ${LIBRAW_DIR}/src/decoders/load_mfbacks.cpp
    ${LIBRAW_DIR}/src/decoders/smal.cpp
    ${LIBRAW_DIR}/src/decoders/unpack_thumb.cpp
    ${LIBRAW_DIR}/src/decoders/unpack.cpp
    ${LIBRAW_DIR}/src/demosaic/aahd_demosaic.cpp
    ${LIBRAW_DIR}/src/demosaic/ahd_demosaic.cpp
    ${LIBRAW_DIR}/src/demosaic/dcb_demosaic.cpp
    ${LIBRAW_DIR}/src/demosaic/dht_demosaic.cpp
    ${LIBRAW_DIR}/src/demosaic/misc_demosaic.cpp
    ${LIBRAW_DIR}/src/demosaic/xtrans_demosaic.cpp
    ${LIBRAW_DIR}/src/integration/dngsdk_glue.cpp
    ${LIBRAW_DIR}/src/integration/rawspeed_glue.cpp
    ${LIBRAW_DIR}/src/metadata/adobepano.cpp
    ${LIBRAW_DIR}/src/metadata/canon.cpp
    ${LIBRAW_DIR}/src/metadata/ciff.cpp
    ${LIBRAW_DIR}/src/metadata/cr3_parser.cpp
    ${LIBRAW_DIR}/src/metadata/epson.cpp
    ${LIBRAW_DIR}/src/metadata/exif_gps.cpp
    ${LIBRAW_DIR}/src/metadata/fuji.cpp
    ${LIBRAW_DIR}/src/metadata/identify_tools.cpp
    ${LIBRAW_DIR}/src/metadata/identify.cpp
    ${LIBRAW_DIR}/src/metadata/kodak.cpp
    ${LIBRAW_DIR}/src/metadata/leica.cpp
    ${LIBRAW_DIR}/src/metadata/makernotes.cpp
    ${LIBRAW_DIR}/src/metadata/mediumformat.cpp
    ${LIBRAW_DIR}/src/metadata/minolta.cpp
    ${LIBRAW_DIR}/src/metadata/misc_parsers.cpp
    ${LIBRAW_DIR}/src/metadata/nikon.cpp
    ${LIBRAW_DIR}/src/metadata/normalize_model.cpp
    ${LIBRAW_DIR}/src/metadata/olympus.cpp
    ${LIBRAW_DIR}/src/metadata/hasselblad_model.cpp
    ${LIBRAW_DIR}/src/metadata/p1.cpp
    ${LIBRAW_DIR}/src/metadata/pentax.cpp
    ${LIBRAW_DIR}/src/metadata/samsung.cpp
    ${LIBRAW_DIR}/src/metadata/sony.cpp
    ${LIBRAW_DIR}/src/metadata/tiff.cpp
    ${LIBRAW_DIR}/src/postprocessing/aspect_ratio.cpp
    ${LIBRAW_DIR}/src/postprocessing/dcraw_process.cpp
    ${LIBRAW_DIR}/src/postprocessing/mem_image.cpp
    ${LIBRAW_DIR}/src/postprocessing/postprocessing_aux.cpp
    ${LIBRAW_DIR}/src/postprocessing/postprocessing_utils_dcrdefs.cpp
    ${LIBRAW_DIR}/src/postprocessing/postprocessing_utils.cpp
    ${LIBRAW_DIR}/src/preprocessing/ext_preprocess.cpp
    ${LIBRAW_DIR}/src/preprocessing/raw2image.cpp
    ${LIBRAW_DIR}/src/preprocessing/subtract_black.cpp
    ${LIBRAW_DIR}/src/tables/cameralist.cpp
    ${LIBRAW_DIR}/src/tables/colorconst.cpp
    ${LIBRAW_DIR}/src/tables/colordata.cpp
    ${LIBRAW_DIR}/src/tables/wblists.cpp
    ${LIBRAW_DIR}/src/utils/curves.cpp
    ${LIBRAW_DIR}/src/utils/decoder_info.cpp
    ${LIBRAW_DIR}/src/utils/init_close_utils.cpp
    ${LIBRAW_DIR}/src/utils/open.cpp
    ${LIBRAW_DIR}/src/utils/phaseone_processing.cpp
    ${LIBRAW_DIR}/src/utils/read_utils.cpp
    ${LIBRAW_DIR}/src/utils/thumb_utils.cpp
    ${LIBRAW_DIR}/src/utils/utils_dcraw.cpp
    ${LIBRAW_DIR}/src/utils/utils_libraw.cpp
    ${LIBRAW_DIR}/src/write/apply_profile.cpp
    ${LIBRAW_DIR}/src/write/file_write.cpp
    ${LIBRAW_DIR}/src/write/tiff_writer.cpp
    ${LIBRAW_DIR}/src/x3f/x3f_parse_process.cpp
    ${LIBRAW_DIR}/src/x3f/x3f_utils_patched.cpp
)
target_include_directories(libraw PUBLIC ${LIBRAW_DIR})
# Not -DLIBRAW_NOTHREADS (that's the OTHER, single-threaded-only
# variant Makefile.am also defines, lib_libraw_a_CPPFLAGS) - familiar's
# own image-loading can run more than one ThreadedIO worker over its
# lifetime, so this builds LibRaw's thread-safe/reentrant variant
# instead (Makefile.am's lib_libraw_r_a_CXXFLAGS: -pthread, no
# LIBRAW_NOTHREADS define).
find_package(Threads REQUIRED)
target_link_libraries(libraw PUBLIC Threads::Threads)
# MSVC only, and NOT optional there: libraw.h auto-defines
# LIBRAW_WIN32_DLLDEFS for _MSC_VER/__INTEL_COMPILER, after which
# libraw_types.h stamps every API declaration with __declspec(dllimport)
# unless told otherwise - i.e. the headers assume LibRaw is being
# CONSUMED as a DLL. This target is a STATIC library, so compiling its
# own sources against dllimport declarations fails outright
# ("error C2491: definition of dllimport function not allowed", hundreds
# of them - a real Windows CI failure; Linux/macOS never hit it because
# LIBRAW_WIN32_DLLDEFS isn't defined there at all).
#
# PUBLIC, not PRIVATE: familiar's own fileio.cpp includes libraw/libraw.h
# too, and both sides have to agree on what DllDef expands to. With
# PRIVATE, LibRaw itself would build fine while familiar's translation
# units still expected dllimport symbols - trading a compile error for a
# link error.
if (MSVC)
    target_compile_definitions(libraw PUBLIC LIBRAW_NODLL)
endif()
if (UNIX)
    # Matches Makefile.am's own lib_libraw_r_a_CXXFLAGS (-pthread -w) -
    # this project's own -Wall (src/CMakeLists.txt) is only applied to
    # the familiar target itself, not global CMAKE_CXX_FLAGS, so this
    # doesn't suppress anything of ours - just carrying over upstream's
    # own choice for its own (old, C-style, known-noisy) source.
    target_compile_options(libraw PRIVATE -w)
endif()

# Lets CTest see the add_test() registered in src/CMakeLists.txt (needs
# the familiar target to exist first, hence there and not here) - must
# still be called at this top level, CTest ignores add_test() calls from
# a directory enable_testing() never reached.
enable_testing()

add_subdirectory(src)

# CPack
set(CPACK_PACKAGE_VENDOR "try-hard-factory")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "For 2d and 3d artists.")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
# tryhardfactory.org doesn't resolve (confirmed - DNS lookup fails, not
# just "not live yet") - points at the actual repo instead until there's
# a real site. Feeds both the .deb's Homepage: field and, on Windows,
# the WiX installer's About/Help links (CPACK_WIX_PROPERTY_ARP* below).
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/try-hard-factory/familiar")
set(CPACK_PACKAGE_CONTACT "max <mpano91@gmail.com>")
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/data/img/app/familiar_256.png") # TODO: Can any generator make
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
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/data/img/app/familiar.ico")
    # Supported languages can be found at http://wixtoolset.org/documentation/manual/v3/wixui/wixui_localization.html
    # set(CPACK_WIX_CULTURES "ar-SA,bg-BG,ca-ES,hr-HR,cs-CZ,da-DK,nl-NL,en-US,et-EE,fi-FI,fr-FR,de-DE")
    # CPACK_WIX_UI_BANNER/UI_DIALOG deliberately unset - the bitmaps that
    # used to be here were still Flameshot's own (flame logo + literal
    # "Flameshot" text, confirmed by opening them), inherited from the
    # fork and never replaced. Leaving these unset falls back to WiX's
    # own plain default banner/dialog instead of shipping someone else's
    # branding - revisit with real Familiar artwork later.
    set(CPACK_WIX_PROPERTY_ARPHELPLINK "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_WIX_ROOT_FEATURE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")
    set(CPACK_WIX_LIGHT_EXTRA_FLAGS "-dcl:high") # set high compression
    # .fml file association - see the file for caveats (unverified, no
    # Windows/WiX toolchain here to confirm the CPack-generated ids it
    # targets).
    set(CPACK_WIX_PATCH_FILE "${CMAKE_SOURCE_DIR}/packaging/win-installer/fml-file-association.xml")

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
