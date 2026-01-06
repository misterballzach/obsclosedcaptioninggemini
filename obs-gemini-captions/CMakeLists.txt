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

# --- SAFETY CHECK: Detect MSVC + MinGW Qt Mismatch ---
if(MSVC)
    if("${Qt6_DIR}" MATCHES "mingw")
        message(FATAL_ERROR
            "\n\n"
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
            "CRITICAL CONFIGURATION ERROR DETECTED:\n"
            "You are using the MSVC Compiler (Visual Studio), but you have pointed CMake to a MinGW version of Qt.\n"
            "\n"
            "Your Qt path: ${Qt6_DIR}\n"
            "\n"
            "THIS WILL NOT BUILD. The libraries are incompatible.\n"
            "\n"
            "SOLUTION:\n"
            "1. Open the 'Qt Maintenance Tool'.\n"
            "2. Install the 'MSVC 2019 64-bit' (or MSVC 2022) component for Qt 6.\n"
            "3. Re-configure CMake and point Qt6_DIR to the MSVC folder (e.g., C:/Qt/6.x.x/msvc2019_64).\n"
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
            "\n"
        )
    endif()
endif()
# -----------------------------------------------------

include_directories(${LIBOBS_INCLUDE_DIRS})
include_directories(${OBS_FRONTEND_API_INCLUDE_DIRS})

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
