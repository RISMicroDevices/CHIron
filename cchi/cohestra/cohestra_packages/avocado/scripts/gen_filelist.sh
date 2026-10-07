#!/usr/bin/env bash
#*****************************************************************************
# gen_filelist.sh — assemble the Verilator file list for avocado_top.
#
# Usage:
#   AVOCADO_MESH_ROOT=/path/to/mesh/render \
#       gen_filelist.sh [user-dut-rtl-file ...]
#
# AVOCADO_MESH_ROOT must point at the mesh render checkout that provides
# avocado/gen_filelist.sh (patch-validating, rebased file list generator)
# and avocado/rtl/avocado.sv (the Avocado mesh wrapper). Any user DUT RTL
# files passed as arguments are appended to the list.
#
# Output: <this package>/build/avocado_top.f  (mesh + wrapper + incdir +
# user DUT files). avocado_top.sv itself is NOT in the .f — pass it as the
# CMake RTL source instead:
#
#   cmake -S <chiron> -B build/cohestra_v3_avocado \
#       -DCOHESTRA_V3_RTL=CUSTOM \
#       -DCOHESTRA_V3_TOP_MODULE=avocado_top \
#       -DCOHESTRA_V3_RTL_SOURCES="<this package>/avocado_top.sv" \
#       -DCOHESTRA_V3_VERILATOR_ARGS="-f;<this package>/build/avocado_top.f"
#
# (the script prints this command with the concrete paths filled in)
#*****************************************************************************
set -euo pipefail

PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

: "${AVOCADO_MESH_ROOT:?set AVOCADO_MESH_ROOT to the mesh render checkout (provides avocado/gen_filelist.sh and avocado/rtl/avocado.sv)}"

MESH_GEN="$AVOCADO_MESH_ROOT/avocado/gen_filelist.sh"
MESH_F="$AVOCADO_MESH_ROOT/avocado/build/avocado.f"

if [ ! -x "$MESH_GEN" ] && [ ! -f "$MESH_GEN" ]; then
    echo "error: $MESH_GEN not found — is AVOCADO_MESH_ROOT the right checkout?" >&2
    exit 1
fi

# 1. Mesh + wrapper file list (also validates the render's Verilator patch).
bash "$MESH_GEN"

if [ ! -f "$MESH_F" ]; then
    echo "error: expected $MESH_F to be produced by the render's generator" >&2
    exit 1
fi

# 2. Compose the avocado_top file list.
OUT_DIR="$PKG_DIR/build"
mkdir -p "$OUT_DIR"
OUT="$OUT_DIR/avocado_top.f"

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
if [ -n "${AVOCADO_DUT_MODULE:-}" ]; then
    DUT_MODULE_ARG=" \\
      -DCOHESTRA_V3_AVOCADO_DUT_MODULE=$AVOCADO_DUT_MODULE"
fi

cat <<EOF

Configure + build the harness with this top as:

  cmake -S "$CHIRON_ROOT/cchi/cohestra/cohestra_v3" \\
      -B "$CHIRON_ROOT/build/cohestra_v3_avocado" \\
      -DCOHESTRA_V3_RTL=CUSTOM \\
      -DCOHESTRA_V3_TOP_MODULE=avocado_top \\
      -DCOHESTRA_V3_RTL_SOURCES="$PKG_DIR/avocado_top.sv" \\
      -DCOHESTRA_V3_VERILATOR_ARGS="-f;$OUT"$DUT_MODULE_ARG
  cmake --build "$CHIRON_ROOT/build/cohestra_v3_avocado" -j

EOF
