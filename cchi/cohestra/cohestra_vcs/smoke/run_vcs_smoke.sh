#!/usr/bin/env bash
#*****************************************************************************
# run_vcs_smoke.sh — B0 environment smoke for the Cohestra VCS harness: the
# go/no-go check for a machine where VCS was just installed. Runs each stage
# in order, reports it, and exits 0 only if the full chain passes.
#
# Stages:
#   PATH       vcs on PATH, or $VCS_HOME/bin/vcs
#   VERSION    vcs -ID runs (toolchain actually executes)
#   LICENSE    SNPSLMD_LICENSE_FILE or LM_LICENSE_FILE is set
#   GCC        host gcc/g++ present (VCS compiles the DPI C++ with it)
#   COMPILE    vcs -sverilog -full64 compiles the smoke top + DPI C++
#   RUN        ./simv_smoke executes
#   PASS       COHESTRA_VCS_SMOKE_PASS found in the simulation log
#   FSDB       (optional, non-fatal; only when VERDI_HOME is set) rebuild with
#              the Verdi PLI, run, and produce vcs_smoke.fsdb
#
# Env knobs:
#   VCS_HOME / PATH            VCS installation
#   VERDI_HOME                 Verdi installation (optional; enables FSDB probe)
#   SNPSLMD_LICENSE_FILE / LM_LICENSE_FILE   license
#   SMOKE_BUILD_DIR            output dir (default: <this dir>/build; point it
#                              at a native filesystem when running from a
#                              synced/network-mounted checkout)
#*****************************************************************************
set -uo pipefail

SMOKE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SMOKE_BUILD_DIR=${SMOKE_BUILD_DIR:-$SMOKE_DIR/build}

fail() {
    echo "COHESTRA_VCS_SMOKE_FAIL(stage=$1): $2" >&2
    exit 1
}

echo "== STAGE PATH: locating VCS =="
VCS_BIN=""
if command -v vcs >/dev/null 2>&1; then
    VCS_BIN="$(command -v vcs)"
elif [ -n "${VCS_HOME:-}" ] && [ -x "$VCS_HOME/bin/vcs" ]; then
    VCS_BIN="$VCS_HOME/bin/vcs"
else
    fail PATH "vcs not found. Set VCS_HOME and/or PATH, e.g.:
  export VCS_HOME=/path/to/synopsys/vcs/<version>
  export PATH=\"\$VCS_HOME/bin:\$PATH\""
fi
echo "  vcs: $VCS_BIN"

echo "== STAGE VERSION: vcs -ID =="
mkdir -p "$SMOKE_BUILD_DIR"
if ! "$VCS_BIN" -full64 -ID > "$SMOKE_BUILD_DIR/vcs_id.log" 2>&1; then
    fail VERSION "vcs -ID failed to execute (see $SMOKE_BUILD_DIR/vcs_id.log)"
fi
grep -m1 -i "vcs" "$SMOKE_BUILD_DIR/vcs_id.log" | sed 's/^/  /' || true

echo "== STAGE LICENSE: license environment =="
if [ -z "${SNPSLMD_LICENSE_FILE:-}" ] && [ -z "${LM_LICENSE_FILE:-}" ]; then
    fail LICENSE "neither SNPSLMD_LICENSE_FILE nor LM_LICENSE_FILE is set.
  Point one of them at the Synopsys license server, e.g.:
  export SNPSLMD_LICENSE_FILE=<port>@<host>"
fi
echo "  SNPSLMD_LICENSE_FILE=${SNPSLMD_LICENSE_FILE:-<unset>}"
echo "  LM_LICENSE_FILE=${LM_LICENSE_FILE:-<unset>}"

# Probe the server with lmstat when possible: the daemon being UP on its port
# while the snpslmd VENDOR daemon is down (e.g. node-lock hostid mismatch) is
# the most common just-installed failure mode; catch it here with a clear
# message instead of VCS's opaque "Vendor daemon is down" at compile time.
LIC_TARGET="${SNPSLMD_LICENSE_FILE:-${LM_LICENSE_FILE:-}}"
LIC_TARGET="$(echo "$LIC_TARGET" | tr ';' ' ' | tr ':' ' ' | grep -oE '[0-9]+@[A-Za-z0-9_.-]+' | head -1 || true)"
LMSTAT="$(ls -1 "$HOME"/Synopsys/scl/*/linux64/bin/lmstat 2>/dev/null | sort -V | tail -1 || true)"
if [ -n "$LIC_TARGET" ] && [ -n "$LMSTAT" ]; then
    echo "  probing $LIC_TARGET with $LMSTAT"
    if "$LMSTAT" -a -c "$LIC_TARGET" > "$SMOKE_BUILD_DIR/lmstat.log" 2>&1; then
        if grep -q 'snpslmd: *UP' "$SMOKE_BUILD_DIR/lmstat.log"; then
            echo "  vendor daemon snpslmd: UP"
        elif grep -q 'license server UP' "$SMOKE_BUILD_DIR/lmstat.log"; then
            fail LICENSE "license server is UP but the snpslmd vendor daemon is DOWN
  (see $SMOKE_BUILD_DIR/lmstat.log). Typical cause: the license is node-locked
  to a hostid this machine no longer has (e.g. WSL2 MAC changed) — restart
  lmgrd with '-l <log>' and check 'snpslmd exited with status'."
        else
            echo "  (lmstat could not confirm server state; continuing)"
        fi
    else
        echo "  (license server $LIC_TARGET unreachable right now; continuing — VCS will report if fatal)"
    fi
else
    echo "  (no lmstat found or no port@host target; skipping probe)"
fi

echo "== STAGE GCC: host C/C++ compiler =="
command -v gcc >/dev/null 2>&1 || fail GCC "gcc not on PATH"
command -v g++ >/dev/null 2>&1 || fail GCC "g++ not on PATH"
echo "  gcc: $(gcc --version | head -1)"
echo "  g++: $(g++ --version | head -1)"

echo "== STAGE COMPILE: vcs -sverilog -full64 (DPI import/export + timing) =="
mkdir -p "$SMOKE_BUILD_DIR"
if ! (cd "$SMOKE_BUILD_DIR" && "$VCS_BIN" -sverilog -full64 -timescale=1ps/1ps \
        "$SMOKE_DIR/vcs_smoke_top.sv" \
        "$SMOKE_DIR/vcs_smoke.cpp" \
        -CFLAGS "-std=c++17" \
        -o simv_smoke > vcs_compile.log 2>&1); then
    fail COMPILE "vcs compile failed (see $SMOKE_BUILD_DIR/vcs_compile.log)"
fi
echo "  simv_smoke built"

echo "== STAGE RUN: ./simv_smoke =="
if ! (cd "$SMOKE_BUILD_DIR" && ./simv_smoke > simv_smoke.log 2>&1); then
    fail RUN "simv_smoke exited non-zero (see $SMOKE_BUILD_DIR/simv_smoke.log)"
fi
echo "  simv_smoke ran to completion"

echo "== STAGE PASS: checking for COHESTRA_VCS_SMOKE_PASS =="
if ! grep -q "COHESTRA_VCS_SMOKE_PASS" "$SMOKE_BUILD_DIR/simv_smoke.log"; then
    grep -m2 "COHESTRA_VCS_SMOKE_FAIL" "$SMOKE_BUILD_DIR/simv_smoke.log" | sed 's/^/  /' >&2 || true
    fail PASS "COHESTRA_VCS_SMOKE_PASS not found in $SMOKE_BUILD_DIR/simv_smoke.log"
fi
echo "  COHESTRA_VCS_SMOKE_PASS found"

# --- Optional non-fatal FSDB sub-probe ---------------------------------------
if [ -n "${VERDI_HOME:-}" ]; then
    echo "== STAGE FSDB (optional): Verdi PLI FSDB dump =="
    if [ ! -f "$VERDI_HOME/share/PLI/VCS/LINUX64/novas.tab" ]; then
        echo "  SKIP/FAIL(non-fatal): $VERDI_HOME/share/PLI/VCS/LINUX64/novas.tab not found"
    else
        mkdir -p "$SMOKE_BUILD_DIR/fsdb"
        if (cd "$SMOKE_BUILD_DIR/fsdb" && "$VCS_BIN" -sverilog -full64 -timescale=1ps/1ps \
                +define+VCS_SMOKE_FSDB \
                -debug_access+all \
                "$SMOKE_DIR/vcs_smoke_top.sv" \
                "$SMOKE_DIR/vcs_smoke.cpp" \
                -CFLAGS "-std=c++17" \
                -o simv_smoke_fsdb > vcs_compile_fsdb.log 2>&1 \
            && ./simv_smoke_fsdb > simv_smoke_fsdb.log 2>&1 \
            && grep -q "COHESTRA_VCS_SMOKE_PASS" simv_smoke_fsdb.log \
            && [ -f vcs_smoke.fsdb ]); then
            echo "  FSDB dump OK: $SMOKE_BUILD_DIR/fsdb/vcs_smoke.fsdb"
        else
            echo "  FAIL(non-fatal): FSDB sub-probe failed (see $SMOKE_BUILD_DIR/fsdb/*.log)"
        fi
    fi
else
    echo "== STAGE FSDB (optional): skipped (VERDI_HOME not set) =="
fi

echo "COHESTRA_VCS_SMOKE_PASS (all stages)"
exit 0
