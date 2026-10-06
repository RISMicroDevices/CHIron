cmake_minimum_required(VERSION 3.21)

cmake_policy(SET CMP0074 NEW)

project(cohestra_v3 LANGUAGES CXX)

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

find_package(verilator REQUIRED)

# Waveform trace format compiled into cohestra_v3.
# Verilator supports only one trace format per verilation; the runtime
# configuration entry 'waveform.type' must match the format compiled here.
set(COHESTRA_V3_WAVEFORM_FORMAT "FST" CACHE STRING "Waveform trace format compiled into cohestra_v3 (FST or VCD)")
set_property(CACHE COHESTRA_V3_WAVEFORM_FORMAT PROPERTY STRINGS FST VCD)

if(COHESTRA_V3_WAVEFORM_FORMAT STREQUAL "FST")
    set(COHESTRA_V3_VERILATE_TRACE TRACE_FST)
elseif(COHESTRA_V3_WAVEFORM_FORMAT STREQUAL "VCD")
    set(COHESTRA_V3_VERILATE_TRACE TRACE_VCD)
else()
    message(FATAL_ERROR "Invalid COHESTRA_V3_WAVEFORM_FORMAT: ${COHESTRA_V3_WAVEFORM_FORMAT} (expected FST or VCD)")
endif()

file(GLOB_RECURSE COHESTRA_V3_SOURCES CONFIGURE_DEPENDS
    "${COHES_TRA_ROOT}/*.cpp"
)
list(FILTER COHESTRA_V3_SOURCES EXCLUDE REGEX "/spdlog/")
list(FILTER COHESTRA_V3_SOURCES EXCLUDE REGEX "/simpleini/")
list(FILTER COHESTRA_V3_SOURCES EXCLUDE REGEX "/build[^/]*/")
list(FILTER COHESTRA_V3_SOURCES EXCLUDE REGEX "/generated/")
list(FILTER COHESTRA_V3_SOURCES EXCLUDE REGEX "/cohestra_earth/")

file(GLOB_RECURSE COHESTRA_V3_HEADERS CONFIGURE_DEPENDS
    "${COHES_TRA_ROOT}/*.h"
    "${COHES_TRA_ROOT}/*.hpp"
)

# RTL selection. Default is the combinational dummy top.
#   -DCOHESTRA_V3_RTL=DUMMY         (default)  top_dummy.sv
#   -DCOHESTRA_V3_RTL=VENUS                    Venus + Verilog AXI slave wrapper
#   -DCOHESTRA_V3_RTL=VENUS_EXTMEM             Venus with AXI ports exposed (C++ AXIMemory)
#   -DCOHESTRA_V3_RTL=CUSTOM                   user sources; requires TOP_MODULE + RTL_SOURCES
set(COHESTRA_V3_RTL "DUMMY" CACHE STRING "Verilated RTL: DUMMY, VENUS, VENUS_EXTMEM, or CUSTOM")
set_property(CACHE COHESTRA_V3_RTL PROPERTY STRINGS DUMMY VENUS VENUS_EXTMEM CUSTOM)
set(COHESTRA_V3_TOP_MODULE "" CACHE STRING "Verilator TOP_MODULE (required for CUSTOM)")
set(COHESTRA_V3_RTL_SOURCES "" CACHE STRING "Semicolon-separated SystemVerilog/Verilog sources (required for CUSTOM)")
set(COHESTRA_V3_PREFIX "" CACHE STRING "Optional Verilator PREFIX override")
set(COHESTRA_V3_ENABLE_WALL OFF CACHE BOOL "Enable -Wall/-Wextra/-Wpedantic (or /W4 on MSVC)")

# Venus DUT geometry knobs (VENUS / VENUS_EXTMEM presets only; applied to the
# top module via Verilator -G). Ranges: NUM_T1 1..8, NUM_AXI 1..4,
# PARALLELISM >= 1. Defaults reproduce the reference DUT.
set(COHESTRA_V3_VENUS_NUM_T1 "4" CACHE STRING "Venus Type-1 upstream ports (1..8)")
set(COHESTRA_V3_VENUS_NUM_AXI "1" CACHE STRING "Venus AXI downstream channels (1..4)")
set(COHESTRA_V3_VENUS_PARALLELISM "8" CACHE STRING "Venus MSHR tracker slots (>=1)")

# TagAlias (VIPT aliasing) field width, 0 = disabled (legacy) .. 8. Drives BOTH
# the Venus -GTAGALIAS_W parameter (VENUS presets) and the C++ flit
# configuration (COHESTRA_TAGALIAS_WIDTH, all presets), keeping the two in sync.
set(COHESTRA_V3_TAGALIAS_WIDTH "0" CACHE STRING "CCHI TagAlias field width (0 = disabled .. 8)")

# CHI (non-CCHI) flit configuration of the interface layer (Cohestra::CHI).
# Defaults reproduce CHI::Eb::FlitConfiguration<> (the reference DUT geometry).
# For the XSCache Oceanus L2 RN-F links use: NODEID_WIDTH=11, RSVDC=4/4,
# DATA_WIDTH=256, DATACHECK/POISON/MPAM=1.
set(COHESTRA_V3_CHI_NODEID_WIDTH "7" CACHE STRING "CHI NodeID width (7..11)")
set(COHESTRA_V3_CHI_REQ_ADDR_WIDTH "48" CACHE STRING "CHI REQ address width (44..52)")
set(COHESTRA_V3_CHI_REQ_RSVDC_WIDTH "4" CACHE STRING "CHI REQ RSVDC width")
set(COHESTRA_V3_CHI_DAT_RSVDC_WIDTH "4" CACHE STRING "CHI DAT RSVDC width")
set(COHESTRA_V3_CHI_DATA_WIDTH "256" CACHE STRING "CHI data width (128/256/512)")
set(COHESTRA_V3_CHI_DATACHECK_PRESENT "0" CACHE STRING "CHI DAT DataCheck present (0/1)")
set(COHESTRA_V3_CHI_POISON_PRESENT "0" CACHE STRING "CHI DAT Poison present (0/1)")
set(COHESTRA_V3_CHI_MPAM_PRESENT "0" CACHE STRING "CHI MPAM present (0/1)")

# Age-matrix arbitration (Venus presets only): 0 = fixed slot-index priority
# (default, legacy), 1 = transaction-age oldest-first via venus_age_matrix.
set(COHESTRA_V3_AGE_MATRIX "0" CACHE STRING "Venus age-matrix arbitration (0=fixed slot priority, 1=transaction-age oldest-first)")

# Verilator multithreading (--threads N): 0 (default) = legacy single-threaded
# model; N >= 1 builds the mtask-partitioned model with N worker threads.
set(COHESTRA_V3_THREADS "0" CACHE STRING "Verilator worker threads (--threads N; 0 = single-threaded)")

set(COHESTRA_V3_VERILATE_THREADS "")
if(COHESTRA_V3_THREADS GREATER 0)
    set(COHESTRA_V3_VERILATE_THREADS THREADS ${COHESTRA_V3_THREADS})
endif()

# Verilator execution profiling (--prof-exec): emits per-mtask timing records
# at runtime (profile_exec.dat, viewable with verilator_gantt). Measure only.
set(COHESTRA_V3_PROF_EXEC "0" CACHE STRING "Verilator execution profiling (--prof-exec; 0=off, 1=on)")

set(COHESTRA_V3_VERILATE_PROF_EXEC "")
if(COHESTRA_V3_PROF_EXEC)
    set(COHESTRA_V3_VERILATE_PROF_EXEC --prof-exec)
endif()

# Optimization level for the harness + verilated model. Verilator workloads at
# -O0 lose ~9-17x vs -O3 (see doc/cohestra-v3-venus-simperf-report.md), so -O3
# is the default; set to "" to fall back to the toolchain/build-type default.
set(COHESTRA_V3_OPT_LEVEL "3" CACHE STRING "Optimization level for cohestra_v3 (GCC/Clang -O<n>; empty = toolchain default)")

string(TOUPPER "${COHESTRA_V3_RTL}" COHESTRA_V3_RTL_UPPER)

# Venus core RTL sources, shared by the VENUS and VENUS_EXTMEM tops
set(COHESTRA_V3_VENUS_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/venus_pkg.sv"
    "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/venus_fifo.sv"
    "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/venus_id_alloc.sv"
    "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/venus_sram_if.sv"
    "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/venus_sram_box.sv"
    "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/venus_directory.sv"
    "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/venus_snoop_filter.sv"
    "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/venus_axi_map.sv"
    "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/venus_axi_master.sv"
    "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/venus_port.sv"
    "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/venus_mshr.sv"
    "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/venus_age_matrix.sv"
    "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/venus.sv"
)

if(COHESTRA_V3_RTL_UPPER STREQUAL "DUMMY")
    set(COHESTRA_V3_VERILATOR_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/top_dummy.sv"
    )
    set(COHESTRA_V3_VERILATOR_TOP_MODULE "top_dummy")
    set(COHESTRA_V3_VERILATOR_PREFIX_DEFAULT "Vtop_dummy")
elseif(COHESTRA_V3_RTL_UPPER STREQUAL "VENUS")
    set(COHESTRA_V3_VERILATOR_SOURCES
        ${COHESTRA_V3_VENUS_SOURCES}
        "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/tb/venus_axi_mem.sv"
        "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/tb/venus_v3_top.sv"
    )
    set(COHESTRA_V3_VERILATOR_TOP_MODULE "venus_v3_top")
    set(COHESTRA_V3_VERILATOR_PREFIX_DEFAULT "Vvenus")
elseif(COHESTRA_V3_RTL_UPPER STREQUAL "VENUS_EXTMEM")
    set(COHESTRA_V3_VERILATOR_SOURCES
        ${COHESTRA_V3_VENUS_SOURCES}
        "${CMAKE_CURRENT_LIST_DIR}/../../icn/venus/tb/venus_v3_top_extmem.sv"
    )
    set(COHESTRA_V3_VERILATOR_TOP_MODULE "venus_v3_top_extmem")
    set(COHESTRA_V3_VERILATOR_PREFIX_DEFAULT "Vvenus_v3_top_extmem")
elseif(COHESTRA_V3_RTL_UPPER STREQUAL "CUSTOM")
    if(COHESTRA_V3_TOP_MODULE STREQUAL "")
        message(FATAL_ERROR "COHESTRA_V3_RTL=CUSTOM requires -DCOHESTRA_V3_TOP_MODULE=<module>")
    endif()
    if(COHESTRA_V3_RTL_SOURCES STREQUAL "")
        message(FATAL_ERROR "COHESTRA_V3_RTL=CUSTOM requires -DCOHESTRA_V3_RTL_SOURCES=<file;[file...]>")
    endif()
    set(COHESTRA_V3_VERILATOR_SOURCES ${COHESTRA_V3_RTL_SOURCES})
    set(COHESTRA_V3_VERILATOR_TOP_MODULE "${COHESTRA_V3_TOP_MODULE}")
    set(COHESTRA_V3_VERILATOR_PREFIX_DEFAULT "V${COHESTRA_V3_TOP_MODULE}")
else()
    message(FATAL_ERROR "Invalid COHESTRA_V3_RTL: ${COHESTRA_V3_RTL} (expected DUMMY, VENUS, VENUS_EXTMEM, or CUSTOM)")
endif()

if(COHESTRA_V3_PREFIX STREQUAL "")
    set(COHESTRA_V3_VERILATOR_PREFIX "${COHESTRA_V3_VERILATOR_PREFIX_DEFAULT}")
else()
    set(COHESTRA_V3_VERILATOR_PREFIX "${COHESTRA_V3_PREFIX}")
endif()

# Venus geometry -G overrides (the DUMMY/CUSTOM tops have no such parameters).
set(COHESTRA_V3_VERILATOR_GARGS "")
if(COHESTRA_V3_RTL_UPPER MATCHES "^VENUS")
    list(APPEND COHESTRA_V3_VERILATOR_GARGS
        "-GNUM_T1=${COHESTRA_V3_VENUS_NUM_T1}"
        "-GNUM_AXI=${COHESTRA_V3_VENUS_NUM_AXI}"
        "-GPARALLELISM=${COHESTRA_V3_VENUS_PARALLELISM}"
        "-GTAGALIAS_W=${COHESTRA_V3_TAGALIAS_WIDTH}"
        "-GAGE_MATRIX=${COHESTRA_V3_AGE_MATRIX}"
    )
endif()

message(STATUS "cohestra_v3 RTL=${COHESTRA_V3_RTL_UPPER} TOP_MODULE=${COHESTRA_V3_VERILATOR_TOP_MODULE} PREFIX=${COHESTRA_V3_VERILATOR_PREFIX} THREADS=${COHESTRA_V3_THREADS}")
if(COHESTRA_V3_RTL_UPPER MATCHES "^VENUS")
    message(STATUS "cohestra_v3 Venus geometry: NUM_T1=${COHESTRA_V3_VENUS_NUM_T1} NUM_AXI=${COHESTRA_V3_VENUS_NUM_AXI} PARALLELISM=${COHESTRA_V3_VENUS_PARALLELISM} TAGALIAS_W=${COHESTRA_V3_TAGALIAS_WIDTH} AGE_MATRIX=${COHESTRA_V3_AGE_MATRIX}")
endif()

add_executable(cohestra_v3)
target_sources(cohestra_v3 PRIVATE
    ${COHESTRA_V3_SOURCES}
    ${COHESTRA_V3_HEADERS}
)

target_include_directories(cohestra_v3 PRIVATE
    "${COHES_TRA_ROOT}"
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    "${COHES_TRA_ROOT}/spdlog/include"
)

target_link_libraries(cohestra_v3 PRIVATE
    SimpleIni::SimpleIni
    spdlog::spdlog
)

verilate(cohestra_v3
    PREFIX ${COHESTRA_V3_VERILATOR_PREFIX}
    TOP_MODULE ${COHESTRA_V3_VERILATOR_TOP_MODULE}
    SOURCES ${COHESTRA_V3_VERILATOR_SOURCES}
    ${COHESTRA_V3_VERILATE_TRACE}
    ${COHESTRA_V3_VERILATE_THREADS}
    VERILATOR_ARGS ${COHESTRA_V3_VERILATOR_GARGS} ${COHESTRA_V3_VERILATE_PROF_EXEC} -Wno-UNUSEDSIGNAL -Wno-UNUSEDPARAM -Wno-PINCONNECTEMPTY -Wno-IMPORTSTAR -Wno-VARHIDDEN -Wno-BLKSEQ -Wno-WIDTHTRUNC
)

target_compile_definitions(cohestra_v3 PRIVATE
    COHESTRA_V3_VERILATED_TOP_HEADER="${COHESTRA_V3_VERILATOR_PREFIX}.h"
    COHESTRA_V3_VERILATED_TOP_CLASS=${COHESTRA_V3_VERILATOR_PREFIX}
    COHESTRA_V3_WAVEFORM_FORMAT_${COHESTRA_V3_WAVEFORM_FORMAT}
    COHESTRA_TAGALIAS_WIDTH=${COHESTRA_V3_TAGALIAS_WIDTH}
    COHESTRA_V3_THREADS=${COHESTRA_V3_THREADS}
    COHESTRA_CHI_NODEID_WIDTH=${COHESTRA_V3_CHI_NODEID_WIDTH}
    COHESTRA_CHI_REQ_ADDR_WIDTH=${COHESTRA_V3_CHI_REQ_ADDR_WIDTH}
    COHESTRA_CHI_REQ_RSVDC_WIDTH=${COHESTRA_V3_CHI_REQ_RSVDC_WIDTH}
    COHESTRA_CHI_DAT_RSVDC_WIDTH=${COHESTRA_V3_CHI_DAT_RSVDC_WIDTH}
    COHESTRA_CHI_DATA_WIDTH=${COHESTRA_V3_CHI_DATA_WIDTH}
    COHESTRA_CHI_DATACHECK_PRESENT=${COHESTRA_V3_CHI_DATACHECK_PRESENT}
    COHESTRA_CHI_POISON_PRESENT=${COHESTRA_V3_CHI_POISON_PRESENT}
    COHESTRA_CHI_MPAM_PRESENT=${COHESTRA_V3_CHI_MPAM_PRESENT}
)

if(COHESTRA_V3_OPT_LEVEL)
    if(MSVC)
        target_compile_options(cohestra_v3 PRIVATE /O2)
    else()
        target_compile_options(cohestra_v3 PRIVATE -O${COHESTRA_V3_OPT_LEVEL})
    endif()
endif()

if(COHESTRA_V3_ENABLE_WALL)
    if(MSVC)
        target_compile_options(cohestra_v3 PRIVATE /W4 /permissive-)
    else()
        target_compile_options(cohestra_v3 PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
