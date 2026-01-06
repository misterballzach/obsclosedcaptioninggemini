# FindObsFrontendApi.cmake

find_library(OBS_FRONTEND_API_LIB
    NAMES obs-frontend-api
    PATH_SUFFIXES bin/64bit bin/32bit lib
    DOC "Path to obs-frontend-api library"
)

# Usually headers are in the same place as libobs, or in a sibling directory.
# But obs-frontend-api.h is often in 'obs-frontend-api' subfolder of includes.
find_path(OBS_FRONTEND_API_INCLUDE_DIR
    NAMES obs-frontend-api.h
    PATH_SUFFIXES obs-frontend-api
    DOC "Path to obs-frontend-api include directory"
)

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
