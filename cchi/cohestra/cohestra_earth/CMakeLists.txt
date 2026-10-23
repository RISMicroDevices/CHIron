cmake_minimum_required(VERSION 3.21)

cmake_policy(SET CMP0074 NEW)

project(cohestra_earth LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(COHES_TRA_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_USE_STD_FORMAT OFF CACHE BOOL "" FORCE)

add_subdirectory("${COHES_TRA_ROOT}/simpleini" "${CMAKE_CURRENT_BINARY_DIR}/simpleini")
add_subdirectory("${COHES_TRA_ROOT}/spdlog" "${CMAKE_CURRENT_BINARY_DIR}/spdlog")

file(GLOB_RECURSE COHESTRA_EARTH_SOURCES CONFIGURE_DEPENDS
    "${COHES_TRA_ROOT}/*.cpp"
)
list(FILTER COHESTRA_EARTH_SOURCES EXCLUDE REGEX "/spdlog/")
list(FILTER COHESTRA_EARTH_SOURCES EXCLUDE REGEX "/simpleini/")
list(FILTER COHESTRA_EARTH_SOURCES EXCLUDE REGEX "/build/")
list(FILTER COHESTRA_EARTH_SOURCES EXCLUDE REGEX "/generated/")
list(FILTER COHESTRA_EARTH_SOURCES EXCLUDE REGEX "/cohestra_v3/")
list(FILTER COHESTRA_EARTH_SOURCES EXCLUDE REGEX "/cohestra_vcs/")

file(GLOB_RECURSE COHESTRA_EARTH_HEADERS CONFIGURE_DEPENDS
    "${COHES_TRA_ROOT}/*.h"
    "${COHES_TRA_ROOT}/*.hpp"
)

add_executable(cohestra_earth)
target_sources(cohestra_earth PRIVATE
    ${COHESTRA_EARTH_SOURCES}
    ${COHESTRA_EARTH_HEADERS}
)

target_include_directories(cohestra_earth PRIVATE
    "${COHES_TRA_ROOT}"
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    "${COHES_TRA_ROOT}/spdlog/include"
)

target_link_libraries(cohestra_earth PRIVATE
    SimpleIni::SimpleIni
    spdlog::spdlog
    z
)

if(MSVC)
    target_compile_options(cohestra_earth PRIVATE /W4 /permissive-)
else()
    target_compile_options(cohestra_earth PRIVATE -Wall -Wextra -Wpedantic)
endif()
