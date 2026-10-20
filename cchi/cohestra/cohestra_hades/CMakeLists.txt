#*****************************************************************************
# cohestra_hades — CMake build of the Cohestra-over-HADES-Fabric harness.
#
# Sibling of cohestra_v3 (Verilator flow) and cohestra_vcs (VCS DPI flow):
# same C++ harness logic, but the DUT boundary is reached over a HADES-Fabric
# link (demo/oceanus_l2 PACKING.md tokens through hades::TransportIF). There
# is NO find_package(verilator) and no RTL step at all — the DUT lives behind
# the fabric (FPGA over JTAG, or the loopback mock / the verilated rehearsal
# harness on the HADES side).
#
# Products:
#   cohestra_hades_core  STATIC lib — the whole harness minus main(); the
#                        HADES rehearsal test (tests/sim/oceanus_rehearsal)
#                        links it in-process
#   cohestra_hades       the standalone executable
#
# Knob:
#   -DHADES_FABRIC_ROOT=<path to the HADES-Fabric checkout>   (REQUIRED)
#
# The HADES runtime is compiled fresh from its sources (add_subdirectory of
# $HADES_FABRIC_ROOT/runtime) instead of linking the prebuilt archive, so the
# frame codec always matches the headers of the tree being pointed at (the
# wire ver 0x01 -> 0x02 bump is in flight there). Nothing is ever written into
# the HADES-Fabric tree.
#
# Typical configure:
#   cmake -S <chiron>/cchi/cohestra/cohestra_hades -B <build> \
#       -DHADES_FABRIC_ROOT=/home/kumon/Projects/HADES-Fabric
#   cmake --build <build> -j
#*****************************************************************************
cmake_minimum_required(VERSION 3.21)

cmake_policy(SET CMP0074 NEW)

project(cohestra_hades LANGUAGES CXX)

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

# ---- HADES-Fabric ------------------------------------------------------------
set(HADES_FABRIC_ROOT "" CACHE PATH "Path to the HADES-Fabric checkout (REQUIRED)")

if(NOT HADES_FABRIC_ROOT)
    message(FATAL_ERROR
        "HADES_FABRIC_ROOT is not set. Point it at the HADES-Fabric checkout, e.g.\n"
        "  cmake -DHADES_FABRIC_ROOT=/home/kumon/Projects/HADES-Fabric ...")
endif()

if(NOT EXISTS "${HADES_FABRIC_ROOT}/runtime/CMakeLists.txt")
    message(FATAL_ERROR
        "HADES_FABRIC_ROOT='${HADES_FABRIC_ROOT}' has no runtime/CMakeLists.txt — "
        "not a HADES-Fabric checkout?")
endif()

add_subdirectory("${HADES_FABRIC_ROOT}/runtime" "${CMAKE_CURRENT_BINARY_DIR}/hades_runtime")

# yaml-cpp: REQUIRED here (the manifest subset parser is yaml-based); the
# HADES runtime also picks it up for its own manifest.cpp
find_package(yaml-cpp REQUIRED)

# ---- knobs (v3-compatible geometry) -------------------------------------------
# TagAlias (VIPT aliasing) field width, 0 = disabled (legacy) .. 8. The Oceanus
# wrapper carries a 2-bit TagAlias field (PACKING.md ch1 [135:134]); match the
# DUT build with -DCOHESTRA_HADES_TAGALIAS_WIDTH=2 when it matters.
set(COHESTRA_HADES_TAGALIAS_WIDTH "0" CACHE STRING "CCHI TagAlias field width (0 = disabled .. 8)")

# CHI (non-CCHI) flit configuration of the interface layer — consumed only by
# the (null) CHI monitor types and the CLog.B parameters tag on this boundary.
set(COHESTRA_HADES_CHI_NODEID_WIDTH "7" CACHE STRING "CHI NodeID width (7..11)")
set(COHESTRA_HADES_CHI_REQ_ADDR_WIDTH "48" CACHE STRING "CHI REQ address width (44..52)")
set(COHESTRA_HADES_CHI_REQ_RSVDC_WIDTH "4" CACHE STRING "CHI REQ RSVDC width")
set(COHESTRA_HADES_CHI_DAT_RSVDC_WIDTH "4" CACHE STRING "CHI DAT RSVDC width")
set(COHESTRA_HADES_CHI_DATA_WIDTH "256" CACHE STRING "CHI data width (128/256/512)")
set(COHESTRA_HADES_CHI_DATACHECK_PRESENT "0" CACHE STRING "CHI DAT DataCheck present (0/1)")
set(COHESTRA_HADES_CHI_POISON_PRESENT "0" CACHE STRING "CHI DAT Poison present (0/1)")
set(COHESTRA_HADES_CHI_MPAM_PRESENT "0" CACHE STRING "CHI MPAM present (0/1)")

# Optimization level for the harness + runtime (same rationale as cohestra_v3).
set(COHESTRA_HADES_OPT_LEVEL "3" CACHE STRING "Optimization level for cohestra_hades (GCC/Clang -O<n>; empty = toolchain default)")

set(COHESTRA_HADES_ENABLE_WALL OFF CACHE BOOL "Enable -Wall/-Wextra/-Wpedantic (or /W4 on MSVC)")

# ---- sources -------------------------------------------------------------------
file(GLOB_RECURSE COHESTRA_HADES_SOURCES CONFIGURE_DEPENDS
    "${COHES_TRA_ROOT}/*.cpp"
)
list(FILTER COHESTRA_HADES_SOURCES EXCLUDE REGEX "/spdlog/")
list(FILTER COHESTRA_HADES_SOURCES EXCLUDE REGEX "/simpleini/")
list(FILTER COHESTRA_HADES_SOURCES EXCLUDE REGEX "/build[^/]*/")
list(FILTER COHESTRA_HADES_SOURCES EXCLUDE REGEX "/generated/")
list(FILTER COHESTRA_HADES_SOURCES EXCLUDE REGEX "/cohestra_earth/")
# Verilator-only cohestra_v3 sources (the sibling harness entry + trace
# runtime); the rest of cohestra_v3 (arguments, logging, event_upstream,
# interface_axi) is shared and stays in — same split as cohestra_vcs.
list(FILTER COHESTRA_HADES_SOURCES EXCLUDE REGEX "/cohestra_v3/executable\\.cpp$")
list(FILTER COHESTRA_HADES_SOURCES EXCLUDE REGEX "/cohestra_v3/waves\\.cpp$")
# The VCS sibling harness has its own CMake project (and its own DPI/entry
# sources that must never link into this executable).
list(FILTER COHESTRA_HADES_SOURCES EXCLUDE REGEX "/cohestra_vcs/")

file(GLOB_RECURSE COHESTRA_HADES_HEADERS CONFIGURE_DEPENDS
    "${COHES_TRA_ROOT}/*.h"
    "${COHES_TRA_ROOT}/*.hpp"
)

set(COHESTRA_HADES_INCLUDE_DIRS
    "${COHES_TRA_ROOT}"
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/../cohestra_v3"
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    "${CMAKE_CURRENT_LIST_DIR}/../../.."
    "${COHES_TRA_ROOT}/spdlog/include"
    "${HADES_FABRIC_ROOT}/runtime/include"
    "${HADES_FABRIC_ROOT}/runtime/src/transport"
)

set(COHESTRA_HADES_DEFINES
    COHESTRA_TAGALIAS_WIDTH=${COHESTRA_HADES_TAGALIAS_WIDTH}
    COHESTRA_CHI_NODEID_WIDTH=${COHESTRA_HADES_CHI_NODEID_WIDTH}
    COHESTRA_CHI_REQ_ADDR_WIDTH=${COHESTRA_HADES_CHI_REQ_ADDR_WIDTH}
    COHESTRA_CHI_REQ_RSVDC_WIDTH=${COHESTRA_HADES_CHI_REQ_RSVDC_WIDTH}
    COHESTRA_CHI_DAT_RSVDC_WIDTH=${COHESTRA_HADES_CHI_DAT_RSVDC_WIDTH}
    COHESTRA_CHI_DATA_WIDTH=${COHESTRA_HADES_CHI_DATA_WIDTH}
    COHESTRA_CHI_DATACHECK_PRESENT=${COHESTRA_HADES_CHI_DATACHECK_PRESENT}
    COHESTRA_CHI_POISON_PRESENT=${COHESTRA_HADES_CHI_POISON_PRESENT}
    COHESTRA_CHI_MPAM_PRESENT=${COHESTRA_HADES_CHI_MPAM_PRESENT}
)

# ---- cohestra_hades_core (STATIC; everything but main) --------------------------
list(FILTER COHESTRA_HADES_SOURCES EXCLUDE REGEX "/cohestra_hades/executable\\.cpp$")

add_library(cohestra_hades_core STATIC
    ${COHESTRA_HADES_SOURCES}
    ${COHESTRA_HADES_HEADERS}
)

target_include_directories(cohestra_hades_core PUBLIC
    ${COHESTRA_HADES_INCLUDE_DIRS}
)

# zlib (z): record-block compression of the CLog.B writer
# (cchi/cohestra/cohestra_clogb.cpp -> clog/clog_b, header-only)
target_link_libraries(cohestra_hades_core PUBLIC
    SimpleIni::SimpleIni
    spdlog::spdlog
    hades_runtime
    yaml-cpp
    z
)

target_compile_definitions(cohestra_hades_core PUBLIC
    ${COHESTRA_HADES_DEFINES}
)

# ---- cohestra_hades (the executable) --------------------------------------------
add_executable(cohestra_hades
    "${CMAKE_CURRENT_LIST_DIR}/executable.cpp"
)

target_link_libraries(cohestra_hades PRIVATE
    cohestra_hades_core
)

# ---- common compile options -------------------------------------------------------
if(COHESTRA_HADES_OPT_LEVEL)
    if(MSVC)
        target_compile_options(cohestra_hades_core PRIVATE /O2)
        target_compile_options(cohestra_hades      PRIVATE /O2)
    else()
        target_compile_options(cohestra_hades_core PRIVATE -O${COHESTRA_HADES_OPT_LEVEL})
        target_compile_options(cohestra_hades      PRIVATE -O${COHESTRA_HADES_OPT_LEVEL})
    endif()
endif()

if(COHESTRA_HADES_ENABLE_WALL)
    if(MSVC)
        target_compile_options(cohestra_hades_core PRIVATE /W4 /permissive-)
        target_compile_options(cohestra_hades      PRIVATE /W4 /permissive-)
    else()
        target_compile_options(cohestra_hades_core PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(cohestra_hades      PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

message(STATUS "cohestra_hades: HADES_FABRIC_ROOT=${HADES_FABRIC_ROOT}")
message(STATUS "cohestra_hades: TAGALIAS_WIDTH=${COHESTRA_HADES_TAGALIAS_WIDTH} CHI NodeID=${COHESTRA_HADES_CHI_NODEID_WIDTH} DATA=${COHESTRA_HADES_CHI_DATA_WIDTH}")
