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
install(DIRECTORY data/locale DESTINATION "${CMAKE_INSTALL_PREFIX}/obs-plugins/64bit/../data/obs-plugins/obs-gemini-captions")
install(TARGETS obs-gemini-captions DESTINATION "${CMAKE_INSTALL_PREFIX}/obs-plugins/64bit")
