#
CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
PROJECT(CHIron_test_tc_xact_state)

#
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -fsanitize=address")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

#
FILE(GLOB_RECURSE sources src/*.cpp src/*.hpp)

#
ADD_EXECUTABLE(test_tc_xact_state ${sources})

#
TARGET_PRECOMPILE_HEADERS(test_tc_xact_state PUBLIC
    "src/common.hpp"
    "src/tc_common.hpp"
)
