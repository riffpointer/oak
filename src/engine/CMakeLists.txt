# src/engine/CMakeLists.txt
# Oak Engine Library
# Contains node graph, timeline, project, config, and plugin support.
# Note: codec/color/gl/audio have been extracted to independent .so modules.
# This module must NOT compile-link against other .so modules.
# Runtime interaction with other modules is through dlopen + C API only.

file(GLOB_RECURSE ENGINE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

add_library(oakengine SHARED ${ENGINE_SOURCES})

target_include_directories(oakengine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/shared/include
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/third_party/openfx/include
  ${CMAKE_SOURCE_DIR}/third_party/openfx/HostSupport/include
)

target_link_libraries(oakengine PUBLIC
  oakshared
  OfxHost
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Gui
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::OpenGL
)

set_target_properties(oakengine PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# All dependencies are linked explicitly; no dynamic_lookup needed.
# if(APPLE)
#   target_link_options(oakengine PUBLIC -undefined dynamic_lookup)
# endif()
