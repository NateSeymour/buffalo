cmake_minimum_required(VERSION 3.28)
project(buffalo)

set(CMAKE_CXX_STANDARD 23)

# External Libs
include(FetchContent)
FetchContent_Declare(
        ctre
        GIT_REPOSITORY https://github.com/hanickadot/compile-time-regular-expressions.git
        GIT_TAG v3.9.0
)
FetchContent_MakeAvailable(ctre)

FetchContent_Declare(
        GTEST
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG main
)
FetchContent_MakeAvailable(GTEST)

# Buffalo
add_library(buffalo INTERFACE
        include/buffalo/buffalo.h
        include/buffalo/buffalo2.h
)
target_link_libraries(buffalo INTERFACE ctre::ctre)

# Tests
add_executable(buffalo-test
        test/calculator2.test.cpp
)
target_link_libraries(buffalo-test PRIVATE buffalo GTest::gtest_main)
target_include_directories(buffalo-test PRIVATE include)