# engine/CMakeLists.txt
# Olive Engine Library
# Contains node graph, timeline, project, config, codec, and plugin support.

file(GLOB_RECURSE ENGINE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

add_library(oakengine STATIC ${ENGINE_SOURCES})

target_include_directories(oakengine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/codec
  ${CMAKE_SOURCE_DIR}/shared/include
  ${CMAKE_SOURCE_DIR}/third_party/openfx/include
  ${CMAKE_SOURCE_DIR}/third_party/openfx/HostSupport/include
  ${FFMPEG_INCLUDE_DIRS}
  ${OIIO_INCLUDE_DIRS}
  ${OPENEXR_INCLUDES}
  ${OCIO_INCLUDE_DIRS}
)

target_link_libraries(oakengine PUBLIC
  oakshared
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Gui
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::OpenGL
  ${FFMPEG_LIBRARIES}
  ${OIIO_LIBRARIES}
  ${OPENEXR_LIBRARIES}
  ${OCIO_LIBRARIES}
)

if(APPLE)
  target_link_options(oakengine PUBLIC -undefined dynamic_lookup)
endif()
