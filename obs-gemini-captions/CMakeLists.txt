cmake_minimum_required(VERSION 3.16)

project(obs-gemini-captions VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Force MSVC Runtime to MultiThreadedDLL (/MD) for Release builds
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    add_compile_options($<$<CONFIG:Release>:/MD>)
    add_compile_options($<$<CONFIG:Debug>:/MDd>)
endif()

# Add local cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Point to our SDK root
if(NOT DEFINED OBS_SDK_PATH)
    set(OBS_SDK_PATH "C:/obs-sdk")
endif()

set(OBS_SOURCE_DIR "${OBS_SDK_PATH}/source")
set(OBS_BIN_DIR "${OBS_SDK_PATH}/bin/64bit")

message(STATUS "Using OBS Source Dir: ${OBS_SOURCE_DIR}")
message(STATUS "Using OBS Lib Dir: ${OBS_BIN_DIR}")

# Sanity Check for Source Dir (OBS 32+ Layout: frontend/api)
if(NOT EXISTS "${OBS_SOURCE_DIR}/frontend/api/obs-frontend-api.h")
    message(FATAL_ERROR "
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    CRITICAL CONFIGURATION ERROR:
    OBS Source Code is missing or incomplete at:
    ${OBS_SOURCE_DIR}/frontend/api/obs-frontend-api.h

    You MUST run 'setup-sdk.ps1' (as Administrator, in VS2022 x64 CMD)
    to download and install the OBS SDK correctly.

    The script will create C:/obs-sdk/source/frontend/api
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    ")
endif()

# Suppress harmless Vulkan warning if not found
find_package(Vulkan QUIET)

find_package(Qt6 COMPONENTS Core Widgets Network REQUIRED)
find_package(Threads REQUIRED)

# We manually define the library targets because we generated them ourselves
add_library(LibObs::LibObs UNKNOWN IMPORTED)
set_target_properties(LibObs::LibObs PROPERTIES
    IMPORTED_LOCATION "${OBS_BIN_DIR}/obs.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${OBS_SOURCE_DIR}/libobs"
)

add_library(ObsFrontendApi::ObsFrontendApi UNKNOWN IMPORTED)
set_target_properties(ObsFrontendApi::ObsFrontendApi PROPERTIES
    IMPORTED_LOCATION "${OBS_BIN_DIR}/obs-frontend-api.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${OBS_SOURCE_DIR}/frontend/api"
)

set(SOURCES
    src/plugin-main.cpp
    src/audio-capture.cpp
    src/gemini-client.cpp
    src/caption-output.cpp
    src/twitch-bot.cpp
)

set(HEADERS
    src/plugin-main.h
    src/audio-capture.h
    src/gemini-client.h
    src/caption-output.h
    src/twitch-bot.h
)

add_library(obs-gemini-captions MODULE ${SOURCES} ${HEADERS})

target_link_libraries(obs-gemini-captions
    PRIVATE
    LibObs::LibObs
    ObsFrontendApi::ObsFrontendApi
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Threads::Threads
)

# CRITICAL: Wire up include directories exactly as requested by upstream
target_include_directories(obs-gemini-captions PRIVATE
    ${OBS_SOURCE_DIR}/frontend/api
    ${OBS_SOURCE_DIR}/libobs
    src
)

set_target_properties(obs-gemini-captions PROPERTIES PREFIX "")

if(APPLE)
    set_target_properties(obs-gemini-captions PROPERTIES SUFFIX ".so")
elseif(WIN32)
    set_target_properties(obs-gemini-captions PROPERTIES SUFFIX ".dll")
endif()

# Install locale data
# Standard OBS data path: data/obs-plugins/plugin-name
install(DIRECTORY data/locale DESTINATION "data/obs-plugins/obs-gemini-captions")

# Install binary
if(WIN32)
    install(TARGETS obs-gemini-captions DESTINATION "obs-plugins/64bit")
else()
    install(TARGETS obs-gemini-captions DESTINATION "obs-plugins/64bit")
endif()
