cmake_minimum_required(VERSION 3.16)

project(obs-gemini-captions VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(LibObs REQUIRED)
find_package(Qt6 COMPONENTS Core Widgets REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

include_directories(${LIBOBS_INCLUDE_DIRS})

set(SOURCES
    src/plugin-main.cpp
    src/audio-capture.cpp
    src/gemini-client.cpp
    src/caption-output.cpp
)

set(HEADERS
    src/plugin-main.h
    src/audio-capture.h
    src/gemini-client.h
    src/caption-output.h
)

add_library(obs-gemini-captions MODULE ${SOURCES} ${HEADERS})

target_link_libraries(obs-gemini-captions
    PRIVATE
    libobs
    obs-frontend-api
    Qt6::Core
    Qt6::Widgets
    ${CURL_LIBRARIES}
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
    # Linux/Mac usually use lib/obs-plugins or similar, but CMAKE_INSTALL_PREFIX usually points to the OBS root or a staging dir.
    # We'll use a standard relative path.
    install(TARGETS obs-gemini-captions DESTINATION "obs-plugins/64bit")
endif()
