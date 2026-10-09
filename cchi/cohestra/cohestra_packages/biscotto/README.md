# Biscotto — Cohestra V3 package

Mesh bring-up + integration for a 2x2 coherent mesh render (the "Biscotto"
mesh wrapper, module `Biscotto`, provided by the external render checkout).

## Layout

- `biscotto_top.sv` — Verilator-ready integration top (Cohestra V3 CUSTOM RTL).
  Exports: `clock`/`reset` (high-active), 3x Type-1 CCHI ports
  (`cchi_t1p0..2_*`), 1x AXI4 memory master port (`axi_m0_*`, 256-bit,
  harness=slave/memory), 1x AXI4 config slave port (`axi_s0_*`, 256-bit,
  harness=master). Inside: the DUT's 3 packed CHI-E RN ports mate the mesh
  RN-F ports (LDID order 4/12/44), links held always-up, SYSCO requested,
  EVENT handshakes looped back, the mesh's peripheral AXI master parked.
- `integration/biscotto_dut_if.svh` — the DUT port contract as SV macros
  (single source; used by the top, the template, and the checker).
- `integration/biscotto_dut_template.sv` — compilable reference `biscotto_dut`
  shell (parks everything; replace with real RTL).
- `scripts/check_dut.sh <rtl...>` — `verilator --lint-only` contract check of
  a user DUT (no mesh needed).
- `scripts/gen_filelist.sh [rtl...]` — builds the full file list
  (`build/biscotto_top.f`, or `$BISCOTTO_TOP_F` when set — concurrent flows
  give their lists distinct names) and prints the CMake configure command.
  Requires
  `BISCOTTO_MESH_ROOT` pointing at the mesh render checkout (with the
  Verilator patches from its `verilator/` directory applied; the script
  invokes `$BISCOTTO_MESH_ROOT/biscotto/gen_filelist.sh`, which re-bases the
  render's file list and gates on those patches).
- `boot/` — the Biscotto boot sequencer (register programming script replayed
  through the harness AXI master on `axi_s0`; see `boot/README.md`).

## DUT contract (summary)

Module `biscotto_dut` (default name — overridable, see below) with
`clock`/`reset`, 3x `cchi_t1p<P>_*` (exactly the Cohestra V3 CCHI Type-1 pin
contract, as in `cohestra_v3/top_dummy.sv`) and 3x `chi_rn<P>_*` packed CHI-E
RN ports (per channel `flit`/`flitpend`/`flitv`/`lcrdv`; REQ 162b, RSP 73b,
DAT 422b, SNP 115b). The DUT never sees link-active/SYSCO/EVENT — the top
manages them.

The DUT module name is configurable: pass `+define+BISCOTTO_DUT_MODULE=<name>`
to Verilator (in the Cohestra V3 build through the generic
`-DCOHESTRA_V3_VERILATOR_ARGS="+define+BISCOTTO_DUT_MODULE=<name>"` — unlike
Avocado there is no dedicated CMake cache variable), or set the
`BISCOTTO_DUT_MODULE` env var when running `scripts/check_dut.sh` /
`scripts/gen_filelist.sh`. Default is `biscotto_dut`.

## AXI MPAM/poison note (Biscotto-specific)

Biscotto renders with AXI MPAM and AXI poison **disabled** on every AXI port
(config, peripheral and memory; the CHI side keeps MPAM/poison/DATACHECK on).
The corresponding mesh pins exist but are inert, and the `Biscotto` wrapper
ties/opens them internally (inputs `'0`, outputs open), so they do **not**
appear on the wrapper boundary: `PERIPH_AXI_*` and `MEM36_AXI_*` carry no
`*MPAM`/`*POISON` pins and `biscotto_top` references none. AXI DATACHECK
stays enabled, so `WDATACHK`/`RDATACHK` remain live pins and are parked/tied
exactly as in the Avocado integration (WDATACHK open, RDATACHK `'0`).

## Build & boot smoke

```sh
BISCOTTO_MESH_ROOT=/path/to/render \
    scripts/gen_filelist.sh integration/biscotto_dut_template.sv
# then the printed cmake command (build dir build/cohestra_v3_biscotto), then:
./cohestra_v3 --config biscotto_smoke.ini   # from the build dir; example ini
                                            # in build/cohestra_v3_biscotto/
```

## Boot sequencer

The boot sequencer (factory name `Biscotto`, see
`cchi/cohestra/cohestra_bootings/boot_biscotto.hpp`) replays the Biscotto
boot script — HN-F SAM control, HN-F power-up (ON/FAM), RN-SAM hashed target
groups and unstall — as write32/verify-read32 pairs over `axi_s0`. Config
knobs (in the `[boot]` section): `biscotto.axi_master` (harness AXI master
index driving `axi_s0`, default 0) and `biscotto.warmup_cycles` (default
25000, see below).

## Reset & warm-up requirements (inherited from the Avocado bring-up)

The mesh config fabric initializes correctly under Verilator only when the
reset sequence produces a real reset-assertion edge after some unreset
runtime — the PUB credit machinery loads its power-on state behind
asynchronous-reset flops, and starting the simulation with reset already
asserted never fires that edge in the 2-state model (the fabric wedges
permanently: no CHI link comes up, `axi_s0` never answers). The harness
therefore runs a `reset_preamble` (default 1024 cycles with reset
deasserted, `[root] reset_preamble`) before the first reset pulse;
`reset_repeat = 2` works as an alternative (the second pulse's assertion is
the needed edge).

After reset release, the mesh power-up (CCB FAM ramp) takes ~16.5k cycles.
Accesses issued earlier are held off by the boot sequencer's
`[boot] biscotto.warmup_cycles` (default 25000 cycles from boot-stage start);
they would otherwise just wait on `AWREADY` (which is also safe, the ramp
is not disturbed by a pending access).

These numbers are carried over from the Avocado bring-up on the same IP
release (same config fabric, same CCB); re-confirm them on the 2x2 render
with the full boot script write+verify smoke (`biscotto_smoke.ini`).
