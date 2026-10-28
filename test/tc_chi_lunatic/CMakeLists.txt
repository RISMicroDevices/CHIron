cmake_minimum_required(VERSION 3.16)
project(CHIron_test_tc_chi_lunatic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB lunatic_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../chi/icn/lunatic/chi_lunatic_*.cpp")
file(GLOB test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")

add_executable(test_chi_lunatic ${test_sources} ${lunatic_sources})
target_include_directories(test_chi_lunatic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_compile_options(test_chi_lunatic PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Werror;-Wno-empty-body>)

enable_testing()
add_test(NAME chi_lunatic COMMAND test_chi_lunatic)
