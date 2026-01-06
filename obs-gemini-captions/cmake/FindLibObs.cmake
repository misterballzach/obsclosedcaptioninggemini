# FindLibObs.cmake

# Try to find libobs in common Windows/Linux locations if not provided
set(OBS_SEARCH_PATHS
    "C:/Program Files/obs-studio"
    "C:/obs-studio"
    "C:/obs-sdk"
    "/usr/include/obs"
    "/usr/local/include/obs"
)

find_path(LIBOBS_INCLUDE_DIR
    NAMES obs.h
    HINTS ${OBS_SEARCH_PATHS}
    PATH_SUFFIXES libobs include/libobs include
    DOC "Path to libobs include directory (look for folder containing obs.h)"
)

find_library(LIBOBS_LIB
    NAMES obs libobs
    HINTS ${OBS_SEARCH_PATHS}
    PATH_SUFFIXES bin/64bit bin/32bit lib build/libobs/Release
    DOC "Path to obs library (look for obs.lib or libobs.so)"
)

if(LIBOBS_LIB)
    get_filename_component(LIB_EXT "${LIBOBS_LIB}" EXT)
    if(LIB_EXT MATCHES "\\.(h|hpp|c|cpp|txt)$")
        message(FATAL_ERROR "
        !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        CRITICAL CONFIGURATION ERROR:
        You selected a SOURCE CODE file ('${LIBOBS_LIB}') for the Library variable.

        CMake expects a compiled library file (ending in .lib on Windows, .so on Linux).

        This error usually means you downloaded the OBS Source Code ZIP instead of the SDK.
        The Source Code zip does NOT contain the required .lib files.

        SOLUTION:
        1. Go to https://github.com/obsproject/obs-studio/actions
        2. Click the latest successful run.
        3. Scroll down to Artifacts and download 'windows-x64-sdk'.
        4. Point LIBOBS_LIB to 'bin/64bit/obs.lib' inside that extracted SDK folder.
        !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        ")
    endif()
endif()

if(NOT LIBOBS_LIB)
    message(WARNING "
    -------------------------------------------------------------------------
    Could NOT find 'obs.lib' (Windows) or 'libobs.so' (Linux).

    If you are on Windows, you CANNOT just use the OBS Source Code ZIP.
    You MUST have the 'libs' (compiled files).

    RECOMMENDATION:
    1. Download the 'OBS Studio SDK' (or CI Artifacts) which contains .lib files.
    2. OR, Build OBS Studio from source yourself.

    Set LIBOBS_LIB to the path of 'obs.lib'.
    -------------------------------------------------------------------------
    ")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibObs
    REQUIRED_VARS LIBOBS_LIB LIBOBS_INCLUDE_DIR
)

if(LibObs_FOUND)
    set(LIBOBS_LIBRARIES ${LIBOBS_LIB})
    set(LIBOBS_INCLUDE_DIRS ${LIBOBS_INCLUDE_DIR})

    if(NOT TARGET LibObs::LibObs)
        add_library(LibObs::LibObs UNKNOWN IMPORTED)
        set_target_properties(LibObs::LibObs PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${LIBOBS_INCLUDE_DIR}"
            IMPORTED_LOCATION "${LIBOBS_LIB}"
        )
    endif()
endif()
