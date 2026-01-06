# FindObsFrontendApi.cmake

# Try to find obs-frontend-api in common Windows/Linux locations if not provided
set(OBS_SEARCH_PATHS
    "C:/Program Files/obs-studio"
    "C:/obs-studio"
    "C:/obs-sdk"
    "/usr/include/obs"
    "/usr/local/include/obs"
)

# Use LibObs location as a hint if found
if(LIBOBS_INCLUDE_DIR)
    get_filename_component(_OBS_ROOT_FROM_INC "${LIBOBS_INCLUDE_DIR}" DIRECTORY) # Parent of libobs include
    list(APPEND OBS_SEARCH_PATHS "${_OBS_ROOT_FROM_INC}")
    list(APPEND OBS_SEARCH_PATHS "${_OBS_ROOT_FROM_INC}/..") # Parent of include
endif()

find_library(OBS_FRONTEND_API_LIB
    NAMES obs-frontend-api
    HINTS ${OBS_SEARCH_PATHS}
    PATH_SUFFIXES bin/64bit bin/32bit lib build/UI/obs-frontend-api/Release
    DOC "Path to obs-frontend-api library"
)

find_path(OBS_FRONTEND_API_INCLUDE_DIR
    NAMES obs-frontend-api.h
    HINTS ${OBS_SEARCH_PATHS}
    PATH_SUFFIXES obs-frontend-api include/obs-frontend-api UI/obs-frontend-api include
    DOC "Path to obs-frontend-api include directory"
)

if(OBS_FRONTEND_API_LIB)
    get_filename_component(LIB_EXT "${OBS_FRONTEND_API_LIB}" EXT)
    if(LIB_EXT MATCHES "\\.(h|hpp|c|cpp|txt)$")
        message(FATAL_ERROR "
        !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        CRITICAL CONFIGURATION ERROR:
        You selected a SOURCE CODE file ('${OBS_FRONTEND_API_LIB}') for the Frontend API Library.

        CMake expects a compiled library file (ending in .lib on Windows).

        Please download the OBS Studio SDK from GitHub Releases (look for -sdk.zip).
        !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        ")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ObsFrontendApi
    REQUIRED_VARS OBS_FRONTEND_API_LIB OBS_FRONTEND_API_INCLUDE_DIR
)

if(ObsFrontendApi_FOUND)
    set(OBS_FRONTEND_API_LIBRARIES ${OBS_FRONTEND_API_LIB})
    set(OBS_FRONTEND_API_INCLUDE_DIRS ${OBS_FRONTEND_API_INCLUDE_DIR})

    if(NOT TARGET ObsFrontendApi::ObsFrontendApi)
        add_library(ObsFrontendApi::ObsFrontendApi UNKNOWN IMPORTED)
        set_target_properties(ObsFrontendApi::ObsFrontendApi PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OBS_FRONTEND_API_INCLUDE_DIR}"
            IMPORTED_LOCATION "${OBS_FRONTEND_API_LIB}"
        )
    endif()
endif()
