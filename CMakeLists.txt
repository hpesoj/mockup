cmake_minimum_required(VERSION 3.14)

project(Mockup)

include(CTest)

add_executable(mockup_test
  "test/test.cpp"
  "test/test_mockup.cpp"
)

target_compile_features(mockup_test
  PUBLIC
  cxx_std_17
)

target_include_directories(mockup_test
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/test"
)

add_test(
  NAME "mockup_test"
  COMMAND "mockup_test"
)
