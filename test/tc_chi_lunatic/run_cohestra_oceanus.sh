#!/usr/bin/env bash
set -euo pipefail

CHIRON_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
: "${OCEANUS_SNAPSHOT:?set OCEANUS_SNAPSHOT to the detached Stage 9 Oceanus snapshot}"
OCEANUS_SNAPSHOT="$(realpath "$OCEANUS_SNAPSHOT")"
EXPECTED_OCEANUS_REV="${EXPECTED_OCEANUS_REV:-419853c2bd55c8c7cf0774602c6f03d2abac26bc}"
ACTUAL_OCEANUS_REV="$(git -C "$OCEANUS_SNAPSHOT" rev-parse HEAD)"
if [[ "$ACTUAL_OCEANUS_REV" != "$EXPECTED_OCEANUS_REV" ]]; then
    echo "Oceanus snapshot must be $EXPECTED_OCEANUS_REV, got $ACTUAL_OCEANUS_REV" >&2
    exit 2
fi
if [[ -n "$(git -C "$OCEANUS_SNAPSHOT" status --porcelain)" ]]; then
    if [[ "${ALLOW_DIRTY_OCEANUS:-0}" != 1 ]]; then
        echo "Oceanus snapshot has uncommitted source changes" >&2
        git -C "$OCEANUS_SNAPSHOT" status --short >&2
        exit 2
    fi
    echo "Oceanus snapshot is dirty; ALLOW_DIRTY_OCEANUS=1 selected for pre-commit verification." >&2
fi

CONFIG="${CONFIG:-$CHIRON_ROOT/test/tc_chi_lunatic/config.cohestra_oceanus.ini}"
CYCLES="${CYCLES:-1000000}"
NUM_L2="${NUM_L2:-1}"
CHI_NIDS="${CHI_NIDS:-1}"
STAGES="${STAGES:-all}"

export CHIRON="$CHIRON_ROOT"
export DUT=l2multichi
export NUM_L2 NUM_SLICE=2 NO_PERF=1
export CONFIG CYCLES
export EXTRA_MAKE_ARGS="CHI_NIDS=$CHI_NIDS CHI_STYLE=separate CHI_NAMES=cohestra CHI_MON=packed ${EXTRA_MAKE_ARGS:-}"
export EXTRA_CMAKE_ARGS="-DCOHESTRA_V3_CHI_NODEID_WIDTH=11 -DCOHESTRA_V3_CHI_REQ_ADDR_WIDTH=48 -DCOHESTRA_V3_CHI_REQ_RSVDC_WIDTH=4 -DCOHESTRA_V3_CHI_DAT_RSVDC_WIDTH=4 -DCOHESTRA_V3_CHI_DATA_WIDTH=256 -DCOHESTRA_V3_CHI_DATACHECK_PRESENT=1 -DCOHESTRA_V3_CHI_POISON_PRESENT=1 -DCOHESTRA_V3_CHI_MPAM_PRESENT=1 ${EXTRA_CMAKE_ARGS:-}"

cd "$OCEANUS_SNAPSHOT"
# shellcheck disable=SC2086
test/boot.sh openllc $STAGES
