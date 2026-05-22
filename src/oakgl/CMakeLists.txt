# oakgl.so — OpenGL Render Backend
# The ONLY module in the project that knows about OpenGL.
# All external code goes through oak/renderer_api.h (pure C ABI).

cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL)

set(OAKGL_SOURCES
    openglrenderer.cpp
    renderer_api.cpp
)

set(OAKGL_HEADERS
    openglrenderer.h
    shadercode.h
)

add_library(oakgl SHARED ${OAKGL_SOURCES} ${OAKGL_HEADERS})

target_include_directories(oakgl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(oakgl PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::OpenGL
)

# Platform-specific OpenGL frameworks on macOS
if(APPLE)
    find_library(OPENGL_FRAMEWORK OpenGL)
    if(OPENGL_FRAMEWORK)
        target_link_libraries(oakgl PRIVATE ${OPENGL_FRAMEWORK})
    endif()
endif()

set_target_properties(oakgl PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
