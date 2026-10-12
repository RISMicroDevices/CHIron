#!/usr/bin/env bash
#*****************************************************************************
# build.sh — build driver for the Cohestra VCS harness (cchi/cohestra/
# cohestra_vcs): builds the harness archive with CMake, then compiles the SV
# bridge + DUT sources + DPI unit with Synopsys VCS and links simv.
#
# Stages:
#   harness   configure & build the C++ harness archive (libcohestra_vcs.a)
#   simv      VCS elaborate+link of the bridge/DUT/DPI into ./simv
#   all       harness + simv (default)
#
# Env knobs (mirroring the XSCache driver's — scripts/avocado/boot_avocado.sh
# — expectations where they overlap):
#   CHIRON              CHIron checkout
#                       (default: derived from this script's location)
#   AVOCADO_PKG         Avocado integration package
#                       (default: $CHIRON/cchi/cohestra/cohestra_packages/avocado)
#   AVOCADO_MESH_ROOT   Avocado mesh render checkout. Not used by this script
#                       itself (the filelist carries absolute paths); needed by
#                       $AVOCADO_PKG/scripts/gen_filelist.sh to (re)generate
#                       FILELIST (no default — set it to your render checkout)
#   BUILD_DIR           output dir for the harness archive and simv; put it on
#                       a NATIVE filesystem (the XSCache driver passes
#                       $XSCACHE/build/cohestra_vcs_*) — building inside a
#                       synced/network-mounted CHIron tree is very slow
#                       (default: $CHIRON/build/cohestra_vcs)
#   CYCLES              only used by the printed run command (default 200000)
#   COHESTRA_VCS_TOP_MODULE  DUT top module (default avocado_top)
#   COHESTRA_VCS_CHI_MON  CHI monitor taps (mon_chi_rn*) on the DPI boundary:
#                       1/0 (default: 1 for avocado_top, 0 otherwise)
#   AVOCADO_DUT_MODULE  optional user-DUT module override (emitted as
#                       +define+AVOCADO_DUT_MODULE=<name>)
#   FILELIST            DUT/mesh filelist (default $AVOCADO_PKG/build/avocado_top.f;
#                       generate it with $AVOCADO_PKG/scripts/gen_filelist.sh)
#   TOP_SOURCES         DUT top sources (default $AVOCADO_PKG/avocado_top.sv)
#   VCS_HOME            VCS installation (used when `vcs` is not on PATH;
#                       also provides svdpi.h to the CMake configure)
#   VERDI_HOME          Verdi installation; when set, FSDB support is compiled
#                       in (+define+COHESTRA_VCS_FSDB, Verdi PLI linked, and
#                       the harness archive gets COHESTRA_VCS_HAVE_FSDB=1)
#   SNPSLMD_LICENSE_FILE / LM_LICENSE_FILE  VCS license (checked by VCS itself;
#                       smoke/run_vcs_smoke.sh validates it explicitly)
#   EXTRA_VCS_FLAGS     extra flags appended to the vcs command line
#   EXTRA_VCS_CFLAGS    extra flags appended to -CFLAGS
#
# Run (after a successful build):
#   (cd "$BUILD_DIR" && ./simv \
#       +cohestra_config=<config.ini> +cohestra_cycles=${CYCLES:-200000})
# plusargs: +cohestra_vcd=<path> / +cohestra_fsdb=<path> / +cohestra_log_file=
#           <path> / +cohestra_no_log_file (see cohestra_vcs_bridge.sv).
# simv exit status: 0 on harness success; non-zero on harness failure (the
# exact harness exit code — 1 startup/config, 2 verification — is printed as
# COHESTRA_VCS_EXIT_STATUS=<n>).
#
# B0 go/no-go (first VCS run on this machine): smoke/run_vcs_smoke.sh.
#*****************************************************************************
set -euo pipefail

COHESTRA_VCS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHIRON=${CHIRON:-$(cd "$COHESTRA_VCS_DIR/../../../.." && pwd)}
AVOCADO_PKG=${AVOCADO_PKG:-$CHIRON/cchi/cohestra/cohestra_packages/avocado}
COHES_ROOT="$CHIRON/cchi/cohestra"

BUILD_DIR=${BUILD_DIR:-$CHIRON/build/cohestra_vcs}
CYCLES=${CYCLES:-200000}
COHESTRA_VCS_TOP_MODULE=${COHESTRA_VCS_TOP_MODULE:-avocado_top}
# CHI monitor taps on the DPI boundary: default ON for avocado_top (the only
# top exporting mon_chi_rn* pins), OFF otherwise. One knob keeps the SV
# (+define), the DPI TU (-CFLAGS) and the harness archive (CMake option)
# in sync — all three must agree on the wire-struct layout.
COHESTRA_VCS_CHI_MON=${COHESTRA_VCS_CHI_MON:-$([ "$COHESTRA_VCS_TOP_MODULE" = avocado_top ] && echo 1 || echo 0)}
# Monitor tap port count (when CHI_MON=1): number of mon_chi_rn* port sets the
# DUT top really exports (avocado 4, biscotto 3, cornetto 2, l2zj $NUM_L2).
# Emitted as per-port presence defines (COHESTRA_VCS_CHI_MON_P0..P(N-1)) plus
# COHESTRA_VCS_CHI_MON_PORTCOUNT_SET to the SV, DPI-TU and harness-archive
# sides so the DUT boundary stays honest (a 3-tap top has no port 3).
COHESTRA_VCS_CHI_MON_PORTS=${COHESTRA_VCS_CHI_MON_PORTS:-4}
case "$COHESTRA_VCS_CHI_MON_PORTS" in 0|1|2|3|4) ;; *) echo "COHESTRA_VCS_CHI_MON_PORTS must be 0..4 (got $COHESTRA_VCS_CHI_MON_PORTS)" >&2; exit 2 ;; esac
# Per-port define expansion shared by the SV (+define) and C++ (-D) spellings.
chi_mon_port_defines_sv=""
chi_mon_port_defines_cc=""
if [ "$COHESTRA_VCS_CHI_MON" = 1 ]; then
    chi_mon_port_defines_sv="+define+COHESTRA_VCS_CHI_MON_PORTCOUNT_SET"
    chi_mon_port_defines_cc="-DCOHESTRA_VCS_CHI_MON_PORTCOUNT_SET=1"
    for ((p = 0; p < COHESTRA_VCS_CHI_MON_PORTS; p++)); do
        chi_mon_port_defines_sv="$chi_mon_port_defines_sv +define+COHESTRA_VCS_CHI_MON_P$p"
        chi_mon_port_defines_cc="$chi_mon_port_defines_cc -DCOHESTRA_VCS_CHI_MON_P$p=1"
    done
fi
FILELIST=${FILELIST:-$AVOCADO_PKG/build/avocado_top.f}
TOP_SOURCES=${TOP_SOURCES:-$AVOCADO_PKG/avocado_top.sv}
EXTRA_VCS_FLAGS=${EXTRA_VCS_FLAGS:-}
EXTRA_VCS_CFLAGS=${EXTRA_VCS_CFLAGS:-}

HARNESS_BUILD="$BUILD_DIR/harness"

stage_harness() {
    local fsdb_opt=OFF
    if [ -n "${VERDI_HOME:-}" ]; then
        fsdb_opt=ON
    fi
    # CHI monitor taps: the harness archive must agree with the SV/DPI sides
    # on the wire-struct layout; and with the taps present, the harness CHI
    # geometry must match the DUT pins (Oceanus RN-F: NID 11, DATACHECK/
    # POISON/MPAM on) or the monitor/CLog.B decode scrambles fields.
    local chi_mon_args=()
    if [ "$COHESTRA_VCS_CHI_MON" = 1 ]; then
        chi_mon_args=(-DCOHESTRA_VCS_CHI_MON=ON
                      -DCOHESTRA_VCS_CHI_MON_PORTS="$COHESTRA_VCS_CHI_MON_PORTS"
                      -DCOHESTRA_VCS_CHI_NODEID_WIDTH=11
                      -DCOHESTRA_VCS_CHI_DATACHECK_PRESENT=1
                      -DCOHESTRA_VCS_CHI_POISON_PRESENT=1
                      -DCOHESTRA_VCS_CHI_MPAM_PRESENT=1)
    fi
    cmake -S "$COHESTRA_VCS_DIR" -B "$HARNESS_BUILD" \
        -DCOHESTRA_VCS_FSDB=$fsdb_opt \
        "${chi_mon_args[@]}"
    cmake --build "$HARNESS_BUILD" -j"$(nproc)"
    [ -f "$HARNESS_BUILD/libcohestra_vcs.a" ] \
        || { echo "error: $HARNESS_BUILD/libcohestra_vcs.a not produced"; exit 1; }
}

stage_simv() {
    # --- VCS toolchain detection ---------------------------------------------
    local vcs_bin=""
    if command -v vcs >/dev/null 2>&1; then
        vcs_bin="$(command -v vcs)"
    elif [ -n "${VCS_HOME:-}" ] && [ -x "$VCS_HOME/bin/vcs" ]; then
        vcs_bin="$VCS_HOME/bin/vcs"
    else
        cat >&2 <<'EOF'
error: VCS not found (neither `vcs` on PATH nor $VCS_HOME/bin/vcs).

Set up the Synopsys VCS environment first, e.g.:
  export VCS_HOME=/path/to/synopsys/vcs/<version>
  export PATH="$VCS_HOME/bin:$PATH"
  export SNPSLMD_LICENSE_FILE=<port>@<host>   # or LM_LICENSE_FILE
  # optional, for FSDB waveforms:
  export VERDI_HOME=/path/to/synopsys/verdi/<version>

Then re-run this script. For a staged go/no-go diagnosis of the environment
(toolchain, license, DPI link, FSDB), run smoke/run_vcs_smoke.sh first.
EOF
        exit 1
    fi
    echo "cohestra_vcs: using VCS: $vcs_bin"

    [ -f "$FILELIST" ] || {
        echo "error: filelist not found: $FILELIST" >&2
        echo "       generate it with: (cd \"$AVOCADO_PKG/scripts\" && AVOCADO_MESH_ROOT=<mesh render> ./gen_filelist.sh <dut rtl files>)" >&2
        exit 1
    }

    [ -f "$HARNESS_BUILD/libcohestra_vcs.a" ] || {
        echo "error: harness archive not found: $HARNESS_BUILD/libcohestra_vcs.a" >&2
        echo "       run: $0 harness" >&2
        exit 1
    }

    local defines="+define+COHESTRA_VCS_TOP_MODULE=$COHESTRA_VCS_TOP_MODULE"
    if [ -n "${AVOCADO_DUT_MODULE:-}" ]; then
        defines="$defines +define+AVOCADO_DUT_MODULE=$AVOCADO_DUT_MODULE"
    fi
    # CHI monitor taps on the DPI boundary (knob defaulted at the top of this
    # script; keeps SV/DPI/harness struct layout in sync).
    if [ "$COHESTRA_VCS_CHI_MON" = 1 ]; then
        defines="$defines +define+COHESTRA_VCS_CHI_MON $chi_mon_port_defines_sv"
    fi

    local pli_flags=""
    if [ -n "${VERDI_HOME:-}" ]; then
        if [ -f "$VERDI_HOME/share/PLI/VCS/LINUX64/novas.tab" ]; then
            defines="$defines +define+COHESTRA_VCS_FSDB"
            # Verdi 2024.09+: FSDB via -debug_access + VERDI_HOME; the old
            # '-P novas.tab pli.a' PLI link is deprecated and errors at runtime
            pli_flags="-debug_access+all"
            echo "cohestra_vcs: FSDB support enabled (VERDI_HOME=$VERDI_HOME)"
        else
            echo "warning: VERDI_HOME set but novas.tab not found under it; building WITHOUT FSDB support" >&2
        fi
    fi

    local cflags="-std=c++20"
    cflags="$cflags -I$COHESTRA_VCS_DIR"
    cflags="$cflags -I$COHESTRA_VCS_DIR/../cohestra_v3"
    cflags="$cflags -I$COHES_ROOT"
    cflags="$cflags -I$COHES_ROOT/.."
    cflags="$cflags -I$CHIRON"
    cflags="$cflags -I$COHES_ROOT/spdlog/include"
    if [ -n "${VERDI_HOME:-}" ] && [ -n "$pli_flags" ]; then
        cflags="$cflags -DCOHESTRA_VCS_HAVE_FSDB=1"
    fi
    if [ "$COHESTRA_VCS_CHI_MON" = 1 ]; then
        cflags="$cflags -DCOHESTRA_VCS_CHI_MON=1 $chi_mon_port_defines_cc"
    fi
    cflags="$cflags $EXTRA_VCS_CFLAGS"

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    echo "cohestra_vcs: linking simv (top module: $COHESTRA_VCS_TOP_MODULE) ..."
    # -lz: zlib for the CLog.B writer (cohestra_clogb.cpp in the harness archive)
    # shellcheck disable=SC2086
    "$vcs_bin" -sverilog -full64 -timescale=1ps/1ps \
        $defines \
        +incdir+"$COHESTRA_VCS_DIR" \
        "$COHESTRA_VCS_DIR/cohestra_vcs_bridge.sv" \
        $TOP_SOURCES \
        -f "$FILELIST" \
        "$COHESTRA_VCS_DIR/cohestra_vcs_dpi.cpp" \
        "$HARNESS_BUILD/libcohestra_vcs.a" \
        "$HARNESS_BUILD/spdlog/libspdlog.a" \
        -LDFLAGS "-lz" \
        -CFLAGS "$cflags" \
        $pli_flags \
        $EXTRA_VCS_FLAGS \
        -o simv

    cat <<EOF

cohestra_vcs: simv built at $BUILD_DIR/simv

Run it with, e.g.:
  (cd "$BUILD_DIR" && ./simv \\
      +cohestra_config=<your config.ini> +cohestra_cycles=$CYCLES)
  # plus optionally: +cohestra_vcd=dump.vcd / +cohestra_fsdb=dump.fsdb
  #                  +cohestra_log_file=run.log / +cohestra_no_log_file
EOF
}

case ${1:-all} in
    harness)        stage_harness ;;
    simv)           stage_simv ;;
    all)            stage_harness; stage_simv ;;
    *) echo "usage: $0 [all|harness|simv]"; exit 2 ;;
esac
