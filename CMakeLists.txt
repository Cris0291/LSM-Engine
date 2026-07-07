cmake_minimum_required(VERSION 3.20)
project(LSM-Engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ZLIB REQUIRED)

add_library(record_codec src/record/record-encoder.cpp)
target_include_directories(record_codec PUBLIC src/record)
target_link_libraries(record_codec PUBLIC ZLIB::ZLIB)

add_library(wal_lib src/wal/wal.cpp)
target_include_directories(wal_lib PUBLIC src/wal)
target_link_libraries(wal_lib PUBLIC record_codec)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(codec_tests tests/test_record_codec.cpp)
target_link_libraries(codec_tests PRIVATE record_codec GTest::gtest_main)

add_executable(wal_tests tests/test_wal.cpp)
target_link_libraries(wal_tests PRIVATE record_codec wal_lib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(codec_tests)
gtest_discover_tests(wal_tests)

