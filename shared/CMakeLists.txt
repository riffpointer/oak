# shared/CMakeLists.txt
# Olive Shared Static Library
# Contains base types, utilities, and tool classes shared by all modules.

set(SHARED_SOURCES
  src/core/render/audioparams.cpp
  src/core/render/samplebuffer.cpp
  src/core/util/bezier.cpp
  src/core/util/color.cpp
  src/core/util/rational.cpp
  src/core/util/stringutils.cpp
  src/core/util/tests.cpp
  src/core/util/timecodefunctions.cpp
  src/core/util/timerange.cpp
  src/core/util/value.cpp
  src/render/frame.cpp
  src/render/framemanager.cpp
  src/render/subtitleparams.cpp
  src/render/videoparams.cpp
  src/common/commandlineparser.cpp
  src/common/debug.cpp
  src/common/ffmpegutils.cpp
  src/common/filefunctions.cpp
  src/common/html.cpp
  src/common/jobtime.cpp
  src/common/ocioutils.cpp
  src/common/oiioutils.cpp
  src/common/icons.cpp
  src/common/xmlutils.cpp
)

add_library(oakshared STATIC ${SHARED_SOURCES})

target_include_directories(oakshared PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/third_party/openfx/include
  ${FFMPEG_INCLUDE_DIRS}
  ${OIIO_INCLUDE_DIRS}
  ${OPENEXR_INCLUDES}
  ${OCIO_INCLUDE_DIRS}
)

# Private includes for shared/ sources to reference each other with relative paths
target_include_directories(oakshared PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/render
)

target_link_libraries(oakshared PUBLIC
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Gui
  # Qt${QT_VERSION_MAJOR}::Widgets  REMOVED: only qtutils.cpp needs it, moved to app/

  ${FFMPEG_LIBRARIES}
  ${OIIO_LIBRARIES}
  ${OPENEXR_LIBRARIES}
  ${OCIO_LIBRARIES}
)

# On macOS, allow undefined symbols to be resolved at runtime
if(APPLE)
  target_link_options(oakshared PUBLIC -undefined dynamic_lookup)
endif()
