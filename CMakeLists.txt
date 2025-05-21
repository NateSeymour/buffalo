cmake_minimum_required(VERSION 3.28)
project(buffalo VERSION 3.0.2)

set(CMAKE_CXX_STANDARD 23)

# External Libs
include(FetchContent)
FetchContent_Declare(
        ctre
        GIT_REPOSITORY https://github.com/hanickadot/compile-time-regular-expressions.git
        GIT_TAG v3.9.0
)
FetchContent_MakeAvailable(ctre)

# Buffalo
add_library(buffalo INTERFACE
        include/buffalo/buffalo.h
)
target_include_directories(buffalo INTERFACE include)
target_link_libraries(buffalo INTERFACE ctre::ctre)

# Tests
option(BUFFALO_ENABLE_TESTS "Include googletest and enable test target" ON)
if(BUFFALO_ENABLE_TESTS)
    FetchContent_Declare(
            GTEST
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG main
    )
    FetchContent_MakeAvailable(GTEST)

    add_executable(buffalo-test
            test/buffalo.test.cpp
            test/parsers/calculator.cpp
            test/parsers/Parser.h
            test/parsers/list.cpp
    )
    target_link_libraries(buffalo-test PRIVATE buffalo GTest::gtest_main)
    target_include_directories(buffalo-test PRIVATE include)
endif()
