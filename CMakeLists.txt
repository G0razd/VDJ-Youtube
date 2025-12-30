cmake_minimum_required(VERSION 3.10)
project(YoutubePlugin)

set(CMAKE_CXX_STANDARD 17)

# Source files
set(SOURCES
    vdjYoutube.cpp
    vdjYoutube.h
    vdjOnlineSource.h
    vdjPlugin8.h
    YoutubePlugin.rc
)

add_library(YoutubePlugin SHARED ${SOURCES})

# Defines for Windows
if(WIN32)
    target_compile_definitions(YoutubePlugin PRIVATE WIN32 _WINDOWS)
endif()
