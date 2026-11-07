cmake_minimum_required(VERSION 3.16)
project(CHIron_test_tc_chi_lunatic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB lunatic_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../chi/icn/lunatic/chi_lunatic_*.cpp")
file(GLOB test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")
list(FILTER lunatic_sources EXCLUDE REGEX "-KUMON-S1-HALO\\.cpp$")
list(FILTER test_sources EXCLUDE REGEX "-KUMON-S1-HALO\\.(cpp|hpp)$")

set_source_files_properties(${test_sources} PROPERTIES COMPILE_OPTIONS "-Wno-error")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/../../cchi/cohestra/cohestra_lunatic.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cchi/cohestra/cohestra_lunatic_config.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cchi/cohestra/cohestra_lunatic_axi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cchi/cohestra/cohestra_axi.cpp"
    PROPERTIES COMPILE_OPTIONS "-Wno-error=unused-parameter")

add_executable(test_chi_lunatic
    ${test_sources}
    ${lunatic_sources}
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cchi/cohestra/cohestra_lunatic.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cchi/cohestra/cohestra_lunatic_config.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cchi/cohestra/cohestra_lunatic_axi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cchi/cohestra/cohestra_axi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cchi/cohestra/cohestra_config.cpp")
target_include_directories(test_chi_lunatic PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cchi/cohestra"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../cchi/cohestra/spdlog/include")
target_compile_options(test_chi_lunatic PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Werror;-Wno-empty-body>)

enable_testing()
add_test(NAME chi_lunatic COMMAND test_chi_lunatic)
add_test(NAME chi_lunatic_filter_match COMMAND test_chi_lunatic Stage9PCrdReturn)
add_test(NAME chi_lunatic_filter_no_match COMMAND test_chi_lunatic __no_registered_test_matches__)
set_tests_properties(chi_lunatic_filter_no_match PROPERTIES WILL_FAIL TRUE)
