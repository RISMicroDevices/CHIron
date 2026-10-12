#*****************************************************************************
# cohestra_vcs — CMake build of the Cohestra VCS harness archive.
#
# Sibling of cohestra_v3 (Verilator flow): same C++ harness logic, but the DUT
# is simulated by Synopsys VCS and reached over SystemVerilog DPI (see
# README.md). This project produces only the harness static archive
# (libcohestra_vcs.a); the simv link happens in build.sh, which compiles
# cohestra_vcs_dpi.cpp + the bridge SV + the DUT sources with VCS and links
# this archive in.
#
# There is NO find_package(verilator) here and no Verilator dependency: the
# Verilator-only cohestra_v3 sources (executable.cpp, waves.cpp) are excluded
# from the glob, while the simulator-neutral cohestra_v3 support sources
# (arguments.cpp, logging.cpp, event_upstream.cpp, interface_axi.cpp) are
# shared.
#
# Typical configure (build.sh does this):
#   cmake -S <chiron>/cchi/cohestra/cohestra_vcs -B <build>/harness
#   cmake --build <build>/harness -j
#*****************************************************************************
cmake_minimum_required(VERSION 3.21)

cmake_policy(SET CMP0074 NEW)

project(cohestra_vcs LANGUAGES CXX)

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

# NOTE: no find_package(verilator) — this flow is VCS-only. svdpi.h is taken
# from the VCS installation ($VCS_HOME/include) when available; Verilator's
# read-only copy is an accepted fallback (it is only a header).
find_path(COHESTRA_VCS_SVDPI_INCLUDE_DIR
    NAMES svdpi.h
    HINTS "$ENV{VCS_HOME}/include"
          "/usr/local/share/verilator/include/vltstd"
    DOC "Directory containing svdpi.h (VCS: $VCS_HOME/include)"
)
if(NOT COHESTRA_VCS_SVDPI_INCLUDE_DIR)
    message(FATAL_ERROR
        "svdpi.h not found. Set VCS_HOME to the VCS installation, or pass "
        "-DCOHESTRA_VCS_SVDPI_INCLUDE_DIR=<dir-with-svdpi.h>.")
endif()
message(STATUS "cohestra_vcs: svdpi.h from ${COHESTRA_VCS_SVDPI_INCLUDE_DIR}")

# Verilog source selection is a build.sh concern (the DUT is compiled by VCS,
# not here); the harness archive is DUT-independent.
set(COHESTRA_VCS_TAGALIAS_WIDTH "0" CACHE STRING "CCHI TagAlias field width (0 = disabled .. 8); must match the DUT build")
set(COHESTRA_VCS_CHI_NODEID_WIDTH "7" CACHE STRING "CHI NodeID width (7..11)")
set(COHESTRA_VCS_CHI_REQ_ADDR_WIDTH "48" CACHE STRING "CHI REQ address width (44..52)")
set(COHESTRA_VCS_CHI_REQ_RSVDC_WIDTH "4" CACHE STRING "CHI REQ RSVDC width")
set(COHESTRA_VCS_CHI_DAT_RSVDC_WIDTH "4" CACHE STRING "CHI DAT RSVDC width")
set(COHESTRA_VCS_CHI_DATA_WIDTH "256" CACHE STRING "CHI data width (128/256/512)")
set(COHESTRA_VCS_CHI_DATACHECK_PRESENT "0" CACHE STRING "CHI DAT DataCheck present (0/1)")
set(COHESTRA_VCS_CHI_POISON_PRESENT "0" CACHE STRING "CHI DAT Poison present (0/1)")
set(COHESTRA_VCS_CHI_MPAM_PRESENT "0" CACHE STRING "CHI MPAM present (0/1)")

# FSDB waveform support (Verdi PLI). build.sh turns this ON when VERDI_HOME is
# set; it must match the +define+COHESTRA_VCS_FSDB on the VCS command line.
option(COHESTRA_VCS_FSDB "Link-time FSDB support (Verdi PLI present)" OFF)

# CHI monitor taps (mon_chi_rn* shadow members in vcs_top.hpp and the
# from_dut_t wire fields). Must match +define+COHESTRA_VCS_CHI_MON on the VCS
# command line (build.sh keeps them in sync).
option(COHESTRA_VCS_CHI_MON "CHI monitor pins on the VCS/DPI boundary" OFF)

# Per-port presence for the monitor taps: emits COHESTRA_VCS_CHI_MON_P0..P(N-1)
# (vcs_top.hpp members + runtime port traits). Must match the +define set on
# the VCS command line (build.sh keeps them in sync). 4 = all ports.
set(COHESTRA_VCS_CHI_MON_PORTS "4" CACHE STRING "Number of CHI monitor tap ports (0-4)")

# Optimization level for the harness archive (same rationale as cohestra_v3).
set(COHESTRA_VCS_OPT_LEVEL "3" CACHE STRING "Optimization level for cohestra_vcs (GCC/Clang -O<n>; empty = toolchain default)")

set(COHESTRA_VCS_ENABLE_WALL OFF CACHE BOOL "Enable -Wall/-Wextra/-Wpedantic (or /W4 on MSVC)")

file(GLOB_RECURSE COHESTRA_VCS_SOURCES CONFIGURE_DEPENDS
    "${COHES_TRA_ROOT}/*.cpp"
)
list(FILTER COHESTRA_VCS_SOURCES EXCLUDE REGEX "/spdlog/")
list(FILTER COHESTRA_VCS_SOURCES EXCLUDE REGEX "/simpleini/")
list(FILTER COHESTRA_VCS_SOURCES EXCLUDE REGEX "/build[^/]*/")
list(FILTER COHESTRA_VCS_SOURCES EXCLUDE REGEX "/generated/")
list(FILTER COHESTRA_VCS_SOURCES EXCLUDE REGEX "/cohestra_earth/")
# Verilator-only cohestra_v3 sources (the sibling harness entry + trace
# runtime); the rest of cohestra_v3 (arguments, logging, event_upstream,
# interface_axi) is shared and stays in.
list(FILTER COHESTRA_VCS_SOURCES EXCLUDE REGEX "/cohestra_v3/executable\\.cpp$")
list(FILTER COHESTRA_VCS_SOURCES EXCLUDE REGEX "/cohestra_v3/waves\\.cpp$")
# The DPI exchange unit is compiled by VCS on the simv link line (build.sh),
# not into this archive.
list(FILTER COHESTRA_VCS_SOURCES EXCLUDE REGEX "/cohestra_vcs/cohestra_vcs_dpi\\.cpp$")
# The B0 smoke is standalone (run_vcs_smoke.sh compiles it directly with VCS).
list(FILTER COHESTRA_VCS_SOURCES EXCLUDE REGEX "/cohestra_vcs/smoke/")

file(GLOB_RECURSE COHESTRA_VCS_HEADERS CONFIGURE_DEPENDS
    "${COHES_TRA_ROOT}/*.h"
    "${COHES_TRA_ROOT}/*.hpp"
)

add_library(cohestra_vcs STATIC
    ${COHESTRA_VCS_SOURCES}
    ${COHESTRA_VCS_HEADERS}
)

target_include_directories(cohestra_vcs PUBLIC
    "${COHES_TRA_ROOT}"
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/../cohestra_v3"
    "${COHES_TRA_ROOT}/spdlog/include"
    "${COHESTRA_VCS_SVDPI_INCLUDE_DIR}"
)

target_link_libraries(cohestra_vcs PRIVATE
    SimpleIni::SimpleIni
    spdlog::spdlog
)

target_compile_definitions(cohestra_vcs PRIVATE
    COHESTRA_TAGALIAS_WIDTH=${COHESTRA_VCS_TAGALIAS_WIDTH}
    COHESTRA_CHI_NODEID_WIDTH=${COHESTRA_VCS_CHI_NODEID_WIDTH}
    COHESTRA_CHI_REQ_ADDR_WIDTH=${COHESTRA_VCS_CHI_REQ_ADDR_WIDTH}
    COHESTRA_CHI_REQ_RSVDC_WIDTH=${COHESTRA_VCS_CHI_REQ_RSVDC_WIDTH}
    COHESTRA_CHI_DAT_RSVDC_WIDTH=${COHESTRA_VCS_CHI_DAT_RSVDC_WIDTH}
    COHESTRA_CHI_DATA_WIDTH=${COHESTRA_VCS_CHI_DATA_WIDTH}
    COHESTRA_CHI_DATACHECK_PRESENT=${COHESTRA_VCS_CHI_DATACHECK_PRESENT}
    COHESTRA_CHI_POISON_PRESENT=${COHESTRA_VCS_CHI_POISON_PRESENT}
    COHESTRA_CHI_MPAM_PRESENT=${COHESTRA_VCS_CHI_MPAM_PRESENT}
    $<$<BOOL:${COHESTRA_VCS_FSDB}>:COHESTRA_VCS_HAVE_FSDB=1>
    $<$<BOOL:${COHESTRA_VCS_CHI_MON}>:COHESTRA_VCS_CHI_MON=1>
    $<$<BOOL:${COHESTRA_VCS_CHI_MON}>:COHESTRA_VCS_CHI_MON_PORTCOUNT_SET=1>
)

# Per-port monitor-tap presence defines (P0..P(N-1) of COHESTRA_VCS_CHI_MON_PORTS).
if(COHESTRA_VCS_CHI_MON)
    foreach(P RANGE 0 3)
        if(P LESS COHESTRA_VCS_CHI_MON_PORTS)
            target_compile_definitions(cohestra_vcs PRIVATE COHESTRA_VCS_CHI_MON_P${P}=1)
        endif()
    endforeach()
endif()

if(COHESTRA_VCS_OPT_LEVEL)
    if(MSVC)
        target_compile_options(cohestra_vcs PRIVATE /O2)
    else()
        target_compile_options(cohestra_vcs PRIVATE -O${COHESTRA_VCS_OPT_LEVEL})
    endif()
endif()

if(COHESTRA_VCS_ENABLE_WALL)
    if(MSVC)
        target_compile_options(cohestra_vcs PRIVATE /W4 /permissive-)
    else()
        target_compile_options(cohestra_vcs PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
