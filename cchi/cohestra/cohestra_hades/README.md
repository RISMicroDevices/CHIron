# cohestra_hades — Cohestra over HADES-Fabric

The host side of the HADES-Fabric hybrid simulation: a sibling of
`cohestra_v3/` (Verilated model) and `cohestra_vcs/` (VCS DPI), binding
Cohestra's `CCHIInterface` boundary (`cchi/cohestra/cohestra_interface.hpp`)
to a **HADES-Fabric link** (`hades::TransportIF`) instead of simulator pins.

The DUT (XSCache Oceanus L2, `TestTop_L2OpenLLC`) runs behind the fabric on
FPGA (JTAG backend) or in the HADES rehearsal harness (Verilated fabric core
+ wrapper), and the whole above-`Instance` Cohestra machinery — FuzzARI /
Core2Core stimulators, cache-line data monitor, transaction watchdog,
profiler, opcount, CCHI flit logger, simperf, CLog.B — runs unchanged.

```
Instance -> HadesCCHIInterface ─┐
                                ├─ HadesFlitStore (FIFOs/queues, PACKING.md
AXIMemory -> HadesAXISlaveInterface ┘        pack/unpack, echo retire)
                                │
                                ├─ HadesLink (window pump: stage/grant/
                                │             doorbell/poll/harvest/unpack)
                                ▼
                    hades::TransportIF ── loopback mock | JTAG/TCP bridge
```

## Build

```sh
cmake -S cchi/cohestra/cohestra_hades -B <build> \
    -DHADES_FABRIC_ROOT=<path to HADES-Fabric checkout>
cmake --build <build> -j
```

Products: `libcohestra_hades_core.a` (the whole flow minus `main()` — the
HADES rehearsal test links this in-process) and `cohestra_hades`.

The HADES runtime (`hades_runtime`) is compiled **fresh from
`$HADES_FABRIC_ROOT/runtime`** via `add_subdirectory`, not linked from the
prebuilt archive, so the frame codec always matches the headers of the tree
being pointed at (the PROTOCOL.md wire ver 0x01→0x02 bump is in flight there;
`hades/frame.hpp` constants are used, never recomputed). Nothing is written
into the HADES-Fabric tree.

Requirements: cmake ≥ 3.21, a C++20 compiler, system `yaml-cpp` (manifest
parsing) and `zlib`.

## Run

```sh
cohestra_hades -c <config.ini> [-n <target cycles>] [--seed <n>] \
    [--backend loopback|jtag] [--log-file[=<path>]]
```

Exit codes match cohestra_v3: `0` = FINISHED clean, `1` = setup/config error,
`2` = simulation FAILED or cache-line monitor mismatch.

Reference config: `config.hades.ini` (this directory). The demo's intended
config is `demo/oceanus_l2/cohestra/config.oceanus_hades.ini` (HADES-Fabric) —
note that draft's `[stimulator.0] name=/seed=` keys are NOT this executable's
schema: stimulators are instantiated via `[root] stimulator.N = FuzzARI` +
`stimulator.N.enable/upstreams.type1` and configured under `[stimulator.N]`
(v3 schema; `config.hades.ini` shows the working shape). `--seed <n>`
overrides `fuzz.ari.seed` on any `[stimulator.N]` section (run_hardware.sh
passes it).

### `[hades]` section

| key | default | meaning |
|---|---|---|
| `manifest` | — (mandatory) | HADES manifest YAML (channel table); resolved CWD-relative, then INI-dir-relative |
| `backend` | manifest's `transport.backend` | `loopback` (mock FPGA) or `jtag` |
| `jtag_url` | `tcp://127.0.0.1:3122` | jtag_mmio_server.tcl endpoint |
| `window_k` | `32` | credit-lead quantum in target cycles; `1` = lockstep debug |
| `skid_depth` | `0` → `window_k` | max staged-but-unaccepted flits per drive channel (wrapper skids are 2*WINDOW_K — safe) |
| `rx_queue` | `1024` | local c2h flit queue bound per channel (absorbs granule bursts; no DUT backpressure path) |
| `max_quiet_polls` | `2000000` | wedge guard: polls without fabric tick progress ⇒ link failure |

## Host token API findings (HADES-Fabric side)

The brief asked: *by which exact host-side API is per-cycle h2c token content
injected, and c2h tokens observed?* Findings (tree @ 94a8498 + WIP):

- **c2h observation**: `hades::Session::set_token_sink`
  (`runtime/include/hades/session.hpp:55`) — a synchronous hook invoked per
  DATA-channel `TOKEN_BATCH`/`TOKEN_BATCH_WIDE` payload from
  `Session::Impl::handleFrame` (`runtime/src/session.cpp:303-308`).
- **h2c injection**: **does not exist yet.** `Session::Impl::stageTokensUpTo`
  (`runtime/src/session.cpp:213-243`) stages *constant all-ones* tokens; the
  header comment says it plainly: "h2c token VALUES: the R-layer host-model
  hook is VP3 scope; until then the session stages constant all-ones tokens"
  (`runtime/src/session.cpp:20-22`).
- The only per-cycle token *semantics* hook in the tree is on the mock side:
  `hades::DutModel` / `MockFpga` (`runtime/src/transport/mock_fpga.hpp:96-129`)
  — the FPGA playing the DUT, not the host injecting content.

**Consequence**: `cohestra_hades` does not use `hades::Session`. `HadesLink`
drives the frozen M↔B contract `hades::TransportIF`
(`runtime/include/hades/transport.hpp:26-37`) directly with a window pump
mirroring `session.cpp`'s shape (monotone CREDIT_GRANT ceilings leading the
staging frontier, granule-aligned TOKEN_BATCH_WIDE frames, one batched
doorbell per staged cycle, head-of-line c2h drain, TICK_ACK/CSR progress
tracking, bounded polls) — but with real per-cycle token content from the
Cohestra boundary.

Two more API gaps this layer works around (do not "fix" from here; HADES-side
milestones own them):

- `hades::loadManifest` rejects the demo's `backend: jtag`
  (`runtime/src/manifest.cpp:81-86`; `hades::BackendKind` has no `Jtag` value,
  `runtime/include/hades/transport.hpp:41`). `HadesLink` therefore parses a
  minimal manifest subset itself (`LoadManifestSubset`, yaml-cpp).
- The JTAG bridge RTL (`rtl/link/jtag/hades_jtag_bridge.sv`) is **Phase J-0:
  single-beat 64 B frames only**; the Oceanus token batches are multi-segment
  `TOKEN_BATCH_WIDE` frames, so `backend=jtag` today fails loudly at the first
  token batch ("Phase J-1 multi-beat bridge" needed). Control-path MMIO
  (MAGIC/CTRL/descriptors/grants/doorbell) works; `JtagTcpTransport` speaks
  the `jtag_mmio_server.tcl` v1 binary batch protocol.

## The pack/unpack contract

Bit-exact layout: `demo/oceanus_l2/docs/PACKING.md` (GENERATED — the tables in
`hades_packing.hpp` mirror it; re-sync after any RTL re-elaboration).
Six channels; one token per DATA channel per target cycle (PROTOCOL.md §2):

| chan | dir | content |
|---|---|---|
| 1 | h2c | rxevt + rxreq + rxrsp + axi_b + ready drives |
| 2 | h2c | rxdat (UpDAT) |
| 3 | h2c | axi_r |
| 4 | c2h | txsnp + txrsp + axi_aw + axi_ar + accept echo |
| 5 | c2h | txdat (DnDAT) |
| 6 | c2h | axi_w |

### Wrapper contract (verified against rtl/hades_oceanus_wrapper.sv)

The wrapper landed (HADES-Fabric `ea885f1`) with these exact semantics; this
layer matches them point for point:

- **h2c drive (stream-slot skid)**: a token's in-band `valid` *pushes one new
  flit* into the channel's skid FIFO (`hades_oceanus_h2c_skid`, depth
  2*WINDOW_K); a hold register drives the DUT and holds until
  `dut_valid && dut_ready` commits at a ce edge. A flit pushed at edge *p* is
  drivable from interval *p+2* (no token→hold bypass).
- **Accept echo (raw ready)**: ch4 [213:208] carries the six DUT ready pins
  sampled at the tick edge and registered — token *t*'s echo describes the
  edge ending interval *t-1*. The host mirror: the oldest staged flit may be
  retired by token *t*'s echo only when it was staged at cycle ≤ *t−3*
  (`kEchoFireLatency` in `hades_interface.cpp`); echo bits high on idle
  cycles are blocked by this drivability fence (`fenceBlocked`, benign). Pops
  happen **only** on admitted echoes, in FIFO order — the v3 stale-ready
  hazard (`cohestra_v3/interface.hpp:784-797`) generalized over a link.
- **c2h capture**: the wrapper pre-asserts the DUT-facing ready itself (its
  capture-skid not-full; the host-driven ch1 ready tail is consumed but
  UNUSED — this layer ties it to 1). Each tick pops at most one captured flit
  into the registered emission segment: a c2h token's `valid` marks exactly
  one delivered flit, exactly once — enqueue on valid, no host-side ready
  qualification exists. Local queues just absorb them (sized by
  `[hades] rx_queue`).
- **Reset stretch**: the wrapper flushes all skid/hold state and ignores
  pushes for RST_CYCLES=128 gated cycles after `CTRL.reset`. Flits staged
  there would vanish without echo, so the store drives idle tokens only for
  the first `[root] reset_preamble` cycles (the bubble window; floor-raised
  to 128 if configured lower).

## Semantics notes / decisions where the brief left freedom

- **Cycle mapping**: link cycle *k* is staged exactly once, during harness
  cycle *k* (`HadesLink::DriveCycle`), after the tick's pushes landed. CLI
  `--cycles` bounds **target cycles**, 1:1. (v3 compares its half-cycle `time`
  against `maximumCycles`, effectively stopping at `maximumCycles/2` target
  cycles — reads like a v3 quirk; deliberately not replicated.)
- **Windowing**: credit leads staging by `window_k` cycles and stays within
  `max_inflight_windows × window_k` of the fabric's reported completion.
  Staging blocks (bounded, `max_quiet_polls`) when credit is exhausted — the
  link being the bottleneck, never a skip. h2c frames flush only on full
  granules (`wideBatchGranule`, e.g. 20/12/15 tokens for ch1/2/3).
- **c2h latency**: received flits reach the Instance when the fabric's
  emission granules flush (ch4/5/6: 18/13/13 tokens) plus link lag; the last
  < granule cycles of a run are never flushed (phase-1 no-partial-flush
  contract — logged, not an error). Watchdog timeouts must absorb this: the
  demo config's `[watchdog] progress.timeout/xact.timeout = 1000000` exists
  for exactly this reason.
- **Determinism**: per-channel flit order/content is fully deterministic
  (FIFO order + in-order skids + echo retirement). The *cycle-position*
  alignment of flits is exact on loopback/Verilator (fixed polls per tick,
  seeded mock) and stall-invariant by LI-BDN construction, but on real
  hardware it can shift with link wall-time (host-side harvest pacing).
  Strict A/B comparisons belong on the rehearsal backend; on hardware compare
  per-channel streams, not absolute cycle tags.
- **Cycle-exact completion**: the grant pump clamps ceilings to the run
  length (`--cycles`; the session.cpp `run(n)` clamp applied incrementally),
  and `FinalDrain` over-stages a bubble **spill** up to the h2c granule LCM
  (60 for the demo channels) so the last granules flush and the fabric
  completes exactly the staged cycles. c2h tails inside an unfilled emission
  granule (≤ 17 cycles) stay behind per the same no-partial-flush contract —
  logged, not an error.
- **Drain set**: c2h frames are pulled on **every DATA channel, both
  directions** — CREDIT_RETURN (§4.3) is emitted per channel including h2c
  ones, and an undrained frame head-of-line blocks the single shared ring
  (this wedged delivery at cycle 1024 = `credit_return_div` before the fix).
- **Reset**: `[root] reset_preamble` is the post-reset **bubble window**
  (meaningful — see the wrapper contract above; must be ≥ RST_CYCLES=128,
  floor-enforced). `[root] reset_cycles/reset_repeat` are **no-ops** — the
  wrapper's stretcher owns the DUT reset assertion and re-arms on CTRL.reset.
- **Boot stage**: only the `delay` sequencer is registered (the Venus-specific
  avocado/biscotto/cornetto sequencers drive v3 pin interfaces that do not
  exist over this boundary).
- **CLog.B**: the 6-channel map carries no `mon_chi_*` taps, so the writer is
  fed by a zero-port `HadesCHIMonitorNull` — `[clogb] enable = 1` writes a
  valid parameters/topology-only stream (deterministic), and any
  `[clogb] rnf.nodeid.*/snf.nodeid.*` key fails the boundary check as designed.
- **No waves**: there are no local pins to trace; use the HADES-side harness
  (or ILA on hardware) for waveforms. `[verilated] waveform.*` keys are
  ignored (no reader).
- **Buffer depth 0**: v3's unbuffered direct-drive fallback does not exist
  over a link — a 0-depth TX FIFO makes pushes always fail.
- **Field width mismatches** (C++ `CommonFlitConfigurationType1` vs DUT pins):
  pack masks down (SrcID 5→4, Way 4→2, TxnID 7→8 zero-extends, ...), unpack
  zero-extends; any c2h field value that loses bits is counted
  (`fieldTruncations`, warned at end of run). `txsnp.alias` (2 b) has no
  CCHI field — read and dropped.
- **manifest token widths**: trusted at runtime (the pad above PACKING.md's
  used bits, e.g. ch4 216 declared vs 214 used, is simply unused; field
  offsets are identical either way).
- **PACKING.md drift**: the tables in `hades_packing.hpp` are a hand-mirror of
  a GENERATED file — they must be re-synced after every DUT re-elaboration
  (the 2026-10-20 regeneration widened SrcID 1→4, rxdat Opcode 1→2, txrsp
  Resp 1→3 and shifted all downstream offsets; the sync note in
  `hades_packing.hpp` records the tracked revision).

## Files

| file | what |
|---|---|
| `hades_packing.hpp` | LSB-first token bit IO + the PACKING.md offset tables |
| `hades_link.{hpp,cpp}` | manifest subset parse + `HadesLink` window pump |
| `hades_transport.{hpp,cpp}` | transport factory: loopback mock config; `JtagTcpTransport` |
| `hades_interface.{hpp,cpp}` | `HadesFlitStore` + `HadesCCHIInterface` + `HadesAXISlaveInterface` + `HadesCHIMonitorNull` |
| `hades_harness.{hpp,cpp}` | the v3-shaped flow as `RunHarness()` (in the core lib) |
| `executable.cpp` | thin `main()` |
| `config.hades.ini` | reference configuration (see schema note above) |

## Smoke status

Verified on this box (cmake 3.28 / g++ 13.3, C++20,
`-DHADES_FABRIC_ROOT=/home/kumon/Projects/HADES-Fabric`):

- build green (`cohestra_hades` + `libcohestra_hades_core.a`);
- **idle loopback smoke** (stimulators off): 3,000 and 20,000 cycles —
  session opens, windows flow, `staged == granted == completed` exactly,
  c2h frames drained == produced (731/731 at 3K), clean exit 0;
- **lockstep** (`window_k=1`) idle: exit 0, exact completion;
- **FuzzARI-on** loopback (K=32 and K=1, 2,000 cycles): 20 transaction births
  through the pack/stage path, watchdog quiet, exit 0 (requests stay
  outstanding — the loopback mock's DUT model is idle and never responds);
- **dual-run determinism** (idle, same seed): logs byte-identical apart from
  the wall-clock `[perf]` throughput line;
- **boundary semantics probes** (standalone, against a software replica of
  the wrapper skids — the loopback mock's all-zero c2h can't exercise them):
  randomized-ready run of 400 REQ flits — pop order == fire order == push
  order, in-flight cap exact, idle ready echoes never pop, bubble window
  leak-free; bit-level round-trips of the token IO (200K random field
  patterns).
