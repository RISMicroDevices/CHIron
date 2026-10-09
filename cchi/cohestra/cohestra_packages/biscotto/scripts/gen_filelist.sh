#!/usr/bin/env bash
#*****************************************************************************
# gen_filelist.sh — assemble the Verilator file list for biscotto_top.
#
# Usage:
#   BISCOTTO_MESH_ROOT=/path/to/mesh/render \
#       gen_filelist.sh [user-dut-rtl-file ...]
#
# BISCOTTO_MESH_ROOT must point at the mesh render checkout that provides
# biscotto/gen_filelist.sh (patch-validating, rebased file list generator)
# and biscotto/rtl/biscotto.sv (the Biscotto mesh wrapper). Any user DUT RTL
# files passed as arguments are appended to the list.
#
# Output: <this package>/build/biscotto_top.f  (mesh + wrapper + incdir +
# user DUT files), or $BISCOTTO_TOP_F when that env var is set — concurrent
# flows sharing this package give their lists distinct names (the XSCache
# L2 driver boot_biscotto.sh uses biscotto_top_l2biscotto.f).
# biscotto_top.sv itself is NOT in the .f — pass it as the
# CMake RTL source instead:
#
#   cmake -S <chiron> -B build/cohestra_v3_biscotto \
#       -DCOHESTRA_V3_RTL=CUSTOM \
#       -DCOHESTRA_V3_TOP_MODULE=biscotto_top \
#       -DCOHESTRA_V3_RTL_SOURCES="<this package>/biscotto_top.sv" \
#       -DCOHESTRA_V3_VERILATOR_ARGS="-f;<this package>/build/biscotto_top.f"
#
# (the script prints this command with the concrete paths filled in)
#*****************************************************************************
set -euo pipefail

PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

: "${BISCOTTO_MESH_ROOT:?set BISCOTTO_MESH_ROOT to the mesh render checkout (provides biscotto/gen_filelist.sh and biscotto/rtl/biscotto.sv)}"

MESH_GEN="$BISCOTTO_MESH_ROOT/biscotto/gen_filelist.sh"
MESH_F="$BISCOTTO_MESH_ROOT/biscotto/build/biscotto.f"

if [ ! -x "$MESH_GEN" ] && [ ! -f "$MESH_GEN" ]; then
    echo "error: $MESH_GEN not found — is BISCOTTO_MESH_ROOT the right checkout?" >&2
    exit 1
fi

# 1. Mesh + wrapper file list (also validates the render's Verilator patch).
bash "$MESH_GEN"

if [ ! -f "$MESH_F" ]; then
    echo "error: expected $MESH_F to be produced by the render's generator" >&2
    exit 1
fi

# 2. Compose the biscotto_top file list. The output path is overridable via
# BISCOTTO_TOP_F: concurrent flows sharing this package (e.g. the template-DUT
# cohestra_v3_biscotto build and the XSCache L2 driver boot_biscotto.sh) must
# not clobber each other's list — the XSCache driver sets its own name.
OUT=${BISCOTTO_TOP_F:-"$PKG_DIR/build/biscotto_top.f"}
mkdir -p "$(dirname "$OUT")"

{
    cat "$MESH_F"
    echo "+incdir+$PKG_DIR/integration"
    for f in "$@"; do
        realpath "$f"
    done
} > "$OUT"

echo "wrote $OUT ($(grep -c . "$OUT") lines)"

# 3. Print the ready-to-run Cohestra V3 configure command.
CHIRON_ROOT="$(cd "$PKG_DIR/../../../.." && pwd)"

DUT_MODULE_ARG=""
if [ -n "${BISCOTTO_DUT_MODULE:-}" ]; then
    DUT_MODULE_ARG=";+define+BISCOTTO_DUT_MODULE=$BISCOTTO_DUT_MODULE"
fi

cat <<EOF

Configure + build the harness with this top as:

  cmake -S "$CHIRON_ROOT/cchi/cohestra/cohestra_v3" \\
      -B "$CHIRON_ROOT/build/cohestra_v3_biscotto" \\
      -DCOHESTRA_V3_RTL=CUSTOM \\
      -DCOHESTRA_V3_TOP_MODULE=biscotto_top \\
      -DCOHESTRA_V3_RTL_SOURCES="$PKG_DIR/biscotto_top.sv" \\
      -DCOHESTRA_V3_VERILATOR_ARGS="-f;$OUT$DUT_MODULE_ARG"
  cmake --build "$CHIRON_ROOT/build/cohestra_v3_biscotto" -j

EOF
