# oakaudio.so — Audio Processing Backend
# Resampling, mixing, format conversion. Zero Qt dependency.

cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FFMPEG REQUIRED COMPONENTS swresample avfilter avutil)

set(OAKAUDIO_SOURCES
    audio_api.cpp
)

add_library(oakaudio SHARED ${OAKAUDIO_SOURCES})

target_include_directories(oakaudio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(oakaudio PRIVATE
    FFMPEG::swresample
    FFMPEG::avfilter
    FFMPEG::avutil
)

set_target_properties(oakaudio PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
