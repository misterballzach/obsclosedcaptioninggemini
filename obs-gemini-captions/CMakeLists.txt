cmake_minimum_required(VERSION 3.16)

project(obs-gemini-captions VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Add local cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(LibObs REQUIRED)
find_package(ObsFrontendApi REQUIRED) # Using our custom finder or standard if available
find_package(Qt6 COMPONENTS Core Widgets Network REQUIRED)
find_package(Threads REQUIRED)

include_directories(${LIBOBS_INCLUDE_DIRS})
include_directories(${OBS_FRONTEND_API_INCLUDE_DIRS})

# Fallback in case finder misses the parent directory for implicit includes
# If header is at .../include/obs-frontend-api/obs-frontend-api.h and finder returns .../include/obs-frontend-api
# Then <obs-frontend-api.h> works.
# If header is at .../include/obs-frontend-api.h and finder returns .../include
# Then <obs-frontend-api.h> works.
# But sometimes users have .../include/obs-frontend-api/obs-frontend-api.h and use <obs-frontend-api/obs-frontend-api.h>
# We add the parent directory of the found include dir just in case.
get_filename_component(OBS_FRONTEND_API_PARENT_DIR "${OBS_FRONTEND_API_INCLUDE_DIRS}" DIRECTORY)
include_directories(${OBS_FRONTEND_API_PARENT_DIR})

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

target_include_directories(obs-gemini-captions PRIVATE src)

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
