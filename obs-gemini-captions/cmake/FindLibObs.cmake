# FindLibObs.cmake

find_path(LIBOBS_INCLUDE_DIR
    NAMES obs.h
    PATH_SUFFIXES libobs
    DOC "Path to libobs include directory"
)

find_library(LIBOBS_LIB
    NAMES obs libobs
    PATH_SUFFIXES bin/64bit bin/32bit lib
    DOC "Path to obs library"
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
