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
