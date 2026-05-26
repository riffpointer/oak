add_library(oakpluginhost SHARED
  pluginhost_api.cpp
  oakpluginhost_internal.h
)

target_include_directories(oakpluginhost
  PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/shared/include
  ${CMAKE_SOURCE_DIR}/third_party/openfx/include
  ${CMAKE_SOURCE_DIR}/third_party/openfx/HostSupport/include
)

target_link_libraries(oakpluginhost
  PUBLIC
  oakshared
  OfxHost
  Qt${QT_VERSION_MAJOR}::Core
)

set_target_properties(oakpluginhost PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
