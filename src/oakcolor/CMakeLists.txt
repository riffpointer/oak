# oakcolor.so — Color Management Backend
# OpenColorIO wrapper for ACEScg workflow.

cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

set(OAKCOLOR_SOURCES
    color_api.cpp
)

add_library(oakcolor SHARED ${OAKCOLOR_SOURCES})

target_include_directories(oakcolor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${OCIO_INCLUDE_DIRS}
)

target_link_libraries(oakcolor PRIVATE
    Qt6::Core
    ${OCIO_LIBRARIES}
)

set_target_properties(oakcolor PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
