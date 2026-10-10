# Cornetto — Cohestra V3 package

Mesh bring-up + integration for a 1x3 coherent mesh render (the "Cornetto"
mesh wrapper, module `Cornetto`, provided by the external render checkout).

## Layout

- `cornetto_top.sv` — Verilator-ready integration top (Cohestra V3 CUSTOM
  RTL). Exports: `clock`/`reset` (high-active), 2x Type-1 CCHI ports
  (`cchi_t1p0..1_*`), 1x AXI4 memory master port (`axi_m0_*`, 256-bit,
  harness=slave/memory), 1x AXI4 config slave port (`axi_s0_*`, 256-bit,
  harness=master). Inside: the DUT's 2 packed CHI-E RN ports mate the mesh
  RN-F ports (LDID order 16/20), links held always-up, SYSCO requested,
  EVENT handshakes looped back, the mesh's peripheral AXI master parked.
- `integration/cornetto_dut_if.svh` — the DUT port contract as SV macros
  (single source; used by the top, the template, and the checker).
- `integration/cornetto_dut_template.sv` — compilable reference
  `cornetto_dut` shell (parks everything; replace with real RTL).
- `scripts/check_dut.sh <rtl...>` — `verilator --lint-only` contract check of
  a user DUT (no mesh needed).
- `scripts/gen_filelist.sh [rtl...]` — builds the full file list
  (`build/cornetto_top.f`) and prints the CMake configure command. Requires
  `CORNETTO_MESH_ROOT` pointing at the mesh render checkout.
- `boot/` — the Cornetto boot sequencer (register programming script
  replayed through the harness AXI master on `axi_s0`; see `boot/README.md`,
  including the per-register decode table of the boot script).

## DUT contract (summary)

Module `cornetto_dut` (default name — overridable, see below) with
`clock`/`reset`, 2x `cchi_t1p<P>_*` (exactly the Cohestra V3 CCHI Type-1 pin
contract, as in `cohestra_v3/top_dummy.sv`) and 2x `chi_rn<P>_*` packed CHI-E
RN ports (per channel `flit`/`flitpend`/`flitv`/`lcrdv`; REQ 162b, RSP 73b,
DAT 422b, SNP 115b). The DUT never sees link-active/SYSCO/EVENT — the top
manages them.

The DUT module name is configurable: pass `+define+CORNETTO_DUT_MODULE=<name>`
to Verilator (in the Cohestra V3 build:
`-DCOHESTRA_V3_CORNETTO_DUT_MODULE=<name>`), or set the `CORNETTO_DUT_MODULE`
env var when running `scripts/check_dut.sh` / `scripts/gen_filelist.sh`.
Default is `cornetto_dut`.

## Build & boot smoke

```sh
CORNETTO_MESH_ROOT=/path/to/render \
    scripts/gen_filelist.sh integration/cornetto_dut_template.sv
# then the printed cmake command, then:
./cohestra_v3 --config <boot-smoke.ini>   # e.g. XSCache test/
                                          # config.xscache_cornetto.ini
```

## Reset & warm-up requirements (inherited from Avocado, 2026-09)

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
`[boot] cornetto.warmup_cycles` (default 25000 cycles from boot-stage
start); they would otherwise just wait on `AWREADY` (which is also safe, the
ramp is not disturbed by a pending access).
