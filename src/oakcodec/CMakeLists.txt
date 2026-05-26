# oakcodec.so — Media Codec Backend
# FFmpeg + OpenImageIO decoder/encoder, conform manager.

cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Find dependencies
find_package(Qt6 REQUIRED COMPONENTS Core Concurrent)
find_package(FFMPEG REQUIRED
    COMPONENTS
    avutil
    avcodec
    avformat
    avfilter
    swscale
    swresample
)
find_package(OpenImageIO 2.1.12 REQUIRED)

# oakshared is a STATIC library built from shared/
# We reference it via target_link_libraries if available,
# otherwise we include its sources directly.
if(NOT TARGET oakshared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../shared ${CMAKE_BINARY_DIR}/oakshared_codec EXCLUDE_FROM_ALL)
endif()

set(OAKCODEC_SOURCES
    codec_api.cpp
    decoder.cpp
    encoder.cpp
    conformmanager.cpp
    exportcodec.cpp
    exportformat.cpp
    footagedescription.cpp
    planarfiledevice.cpp
    ffmpeg_utils.cpp
    ffmpeg/ffmpegdecoder.cpp
    ffmpeg/ffmpegencoder.cpp
    oiio/oiiodecoder.cpp
    oiio/oiioencoder.cpp
)

set(OAKCODEC_HEADERS
    decoder.h
    encoder.h
    conformmanager.h
    exportcodec.h
    exportformat.h
    footagedescription.h
    loopmode.h
    rendermode.h
    planarfiledevice.h
    ffmpeg/ffmpegdecoder.h
    ffmpeg/ffmpegencoder.h
    oiio/oiiodecoder.h
    oiio/oiioencoder.h
)

add_library(oakcodec SHARED ${OAKCODEC_SOURCES} ${OAKCODEC_HEADERS})

target_include_directories(oakcodec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(oakcodec PRIVATE
    oakshared
    Qt6::Core
    Qt6::Concurrent
    ${OIIO_LIBRARIES}
    FFMPEG::avutil
    FFMPEG::avcodec
    FFMPEG::avformat
    FFMPEG::avfilter
    FFMPEG::swscale
    FFMPEG::swresample
    oakcolor
)

set_target_properties(oakcodec PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
