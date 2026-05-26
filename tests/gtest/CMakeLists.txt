add_executable(olive-gtest
  main.cpp
  c_api_engine_test.cpp
  c_api_codec_test.cpp
  c_api_color_test.cpp
  c_api_renderer_test.cpp
  c_api_integration_test.cpp
)

target_include_directories(
  olive-gtest
  PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(
  olive-gtest
  PRIVATE
  oakengine
  oakcodec
  oakcolor
  oakgl
  oakshared
  GTest::gtest
)

if (MSVC)
  add_test("Olive.gtest" olive-gtest)
else()
  add_test(olive-gtest olive-gtest)
endif()
