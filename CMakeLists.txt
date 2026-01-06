cmake_minimum_required(VERSION 3.16)
project(chfloat LANGUAGES CXX)

option(CHFLOAT_BUILD_TESTS "Build chfloat tests" ON)
option(CHFLOAT_BUILD_BENCHMARKS "Build chfloat benchmarks" ON)

add_library(chfloat INTERFACE)
add_library(chfloat::chfloat ALIAS chfloat)

target_compile_features(chfloat INTERFACE cxx_std_17)

target_include_directories(chfloat INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if (CHFLOAT_BUILD_TESTS)
  enable_testing()
  add_executable(chfloat_tests
    test/test_main.cpp
  )
  target_link_libraries(chfloat_tests PRIVATE chfloat::chfloat)
  add_test(NAME chfloat_tests COMMAND chfloat_tests)
endif()

if (CHFLOAT_BUILD_BENCHMARKS)
  add_executable(chfloat_benchmark
    benchmark/benchmark_main.cpp
  )
  target_link_libraries(chfloat_benchmark PRIVATE chfloat::chfloat)
  target_compile_definitions(chfloat_benchmark PRIVATE
    CHFLOAT_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  )
  target_include_directories(chfloat_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../fast_float/include
  )
endif()
