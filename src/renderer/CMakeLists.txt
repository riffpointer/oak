# oakrenderer - Standalone renderer executable
# Loads oakengine.so, oakgl.so, oakcodec.so, etc. via dlopen at runtime.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

set(RENDERER_SOURCES
    main.cpp
    runtime/oak_engine_runtime.cpp
)

add_executable(oakrenderer ${RENDERER_SOURCES})

target_include_directories(oakrenderer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(oakrenderer PRIVATE
    Qt6::Core
)

set_target_properties(oakrenderer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
