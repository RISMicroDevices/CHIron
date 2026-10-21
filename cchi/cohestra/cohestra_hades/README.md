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

## Self-neutralizing build (opt-in)

Every source file in this layer is wrapped in a guard:

```c
#if defined(COHESTRA_HADES)
#  if !__has_include(<hades/transport.hpp>)
#    error "... set HADES_FABRIC_ROOT"
#  endif
#endif
#if defined(COHESTRA_HADES) || __has_include(<hades/transport.hpp>)
... entire file ...
#endif
```

Outside this layer's own opt-in build every translation unit compiles EMPTY,
so foreign builds that sweep `cchi/cohestra/**` (cohestra_v3's GLOB, the
`cchi/build` Makefile header check) are unaffected by the layer's presence.

**Consumer contract**: to actually compile the layer's content you need
EITHER `COHESTRA_HADES=1` (this CMake exports it PUBLIC on
`cohestra_hades_core` and `cohestra_hades`, so target-based consumers inherit
it) OR the HADES-Fabric headers on the include path
(`$HADES_FABRIC_ROOT/runtime/include` — the rehearsal uses this form, since
it links the archive by file path and does not inherit target defines).
Defining `COHESTRA_HADES` without the headers on the path is a hard `#error`
by design. Visual Studio project files are out of scope for this layer.

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
| `view_latency` | `40` | DUT-cycle view offset: received flits are Instance-visible exactly `tag + view_latency` (floor: maxH2CGranule + maxC2HGranule, clamped) |
| `anchor_l_rsp/dat/b/r` | `0` | extra DUT-cycle delay for response staging slots (0 = production tick + 1) |

## The DUT-cycle-anchored response path (strict replay-determinism)

Hard requirement (spec §1.4/D2): under host congestion the per-cycle token
streams must stay byte-identical — the DUT-side event sequence may depend on
the DUT cycle only, never on when the host happened to harvest a token. The
layer enforces this in three places:

1. **Every received c2h flit is tagged** with the DUT cycle its token was
   unpacked from (the link's unpack cursor — exact, transport-independent).
2. **The anchored view**: a received flit becomes visible to the Instance
   exactly `tag + view_latency` harness cycles after its DUT cycle
   (`GatedFront` in `hades_interface.cpp`), never at harvest-arrival time.
   The link guarantees presence by then: `DriveCycle(k)` stalls staging until
   the c2h tokens through `k + 2 − view_latency` are unpacked (a legal
   stall — the DUT simply stops ticking; never a skip).
3. **Response slots**: every flit the Instance (or AXIMemory) pushes is
   assigned a staging slot at push time,
   `slot = max(tick + 1 + anchor_L_chan, prev_channel_slot + 1)` — monotone
   per drive channel, deterministic given the anchored view. The pack stage
   holds a not-yet-due head idle (no bypass, no reordering). So a snoop
   response / AXI b/r lands at a FIXED DUT cycle computed from deterministic
   state — `TxnID`/`DBID` allocation and FuzzARI's closed loop stay pinned.

Effective stimulus→response DUT-cycle latency is
`view_latency + prep + 1 + anchor_L_chan` where `prep` is the deterministic
Instance/AXIMemory preparation in harness ticks (Taurus snoop processing, the
memory's configured latencies).

### L values and derivation

- **`view_latency` (default 40, INI `[hades] view_latency`)** — the view
  offset. Deadlock floor: at a staging halt the fabric can only reach the
  worst-aligned *flushed* h2c granule frontier (`staged − (maxH2CGranule − 1)`
  = −19 for the demo's 20/12/15), and unpack trails the fabric by up to the
  largest c2h emission granule minus one (−17 for ch4's 18). The throttle's
  `k + 2 − L` must clear that bound, so
  `L ≥ (maxH2CGranule − 1) + (maxC2HGranule − 1) + 2 = 38`; the link clamps
  with a warning. Default 40 — floor + 2, which also absorbs the verilated
  fabric's measured startup offset (below). Larger values deepen the link
  pipelining at the cost of response latency.
- **The throttle's non-circular escape**: the hold is released when the
  fabric reports input starvation *having consumed every flushed h2c token*
  (`STALL_REASON` bit0 ∧ `completed ≥ min flushed granule frontier`) — only
  more staging can feed it then, so the view lag is self-inflicted by the
  hold itself. Releasing cannot fake presence: the delivery gate still serves
  a flit only once it is unpacked AND due. Under pure congestion (deferred
  pushes/pulls) the fabric never satisfies that condition, so the hold keeps
  working as intended.
- **Measured RTL startup quirks** (probe evidence, this layer's README only —
  fixes are HADES-side): on `CTRL.reset`, every c2h `axis_async_fifo` read
  pointer advances by 2 (reset-sync artifact — the same suspicion the T4
  wedge-debug recorded), and the first tick after reset loses one c2h push
  (`tok_in_valid` registers its live mask one ce edge late). Net effect: a
  channel's first batch forms only at `granule + 2` pushes. ch5/6 (granule
  13) absorb it silently; ch4 (granule 18) is the only channel that can be
  held one token short of its first batch when the fabric stalls exactly on a
  staging-granule boundary — precisely the case the view throttle creates.
  These are phantom-offset behaviors, deterministic per reset sequence, so
  wire traces stay byte-identical across runs; the throttle escape above is
  what keeps them from deadlocking the host.
- **`anchor_l_rsp / anchor_l_dat / anchor_l_b / anchor_l_r` (default 0)** —
  extra DUT cycles beyond `production tick + 1` for response slots. 0 is
  causally safe (the production tick already sits `view_latency` past the
  stimulus's harvest); the knobs exist for a future DUT/wrapper that wants a
  guaranteed longer response latency.

Everything else in this document (the window pump, the skid/echo contract,
the reset bubble window, cycle-exact completion) is unchanged.

### strict-AB status note (2026-10-20, W4 rehearsal @ 5080c85)

With the anchoring above, the rehearsal's `--strict-ab` at 100K shows the
**h2c direction byte-identical** (11,635,818 B) and the **c2h per-channel
token content byte-identical** (99,990/99,996 tokens per channel, zero
content diffs). The one residual divergence is the **c2h frame ORDER on the
shared emission stream** (first diff at byte offset 25850 = frame 50: a
ch4 t1_resp_out batch vs a ch6 axi_w_out batch swapped between the reference
and the stall-injected run). Root cause is fabric-side, not this layer:
`hades_fabric_core.sv`'s emission arbiter services the lowest channel with a
pending batch when its pack FSM is free, so the inter-channel order follows
input-arrival bursts, which stall injection perturbs by construction. A
canonical fix is to serialize c2h batches in granule-completion-tick order
(a completion-order FIFO) inside the fabric — the host cannot affect the
recorded wire order, and per-channel streams already carry full content
identity.

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
- **Windowing**: the grant staircase is purely staged-paced — credit leads
  the staging frontier by `grantLead = max(W × window_k, maxGranule + window_k)`
  cycles, **independent of the fabric's reported completion** (strict per-cycle
  wire identity requires grant frames to hold their trace positions under
  congestion; bounding the pipeline is the view throttle's job — staging never
  outruns the fabric by more than ~`view_latency` cycles, which also keeps the
  fabric's input FIFO and the mock's over-send check safe). Staging blocks
  (bounded, `max_quiet_polls`) only on the view throttle — the link being
  congested, never a skip. h2c frames flush only on full granules
  (`wideBatchGranule`, e.g. 20/12/15 tokens for ch1/2/3).
- **c2h latency**: received flits reach the Instance exactly `view_latency`
  cycles after their DUT cycle (the anchored view; the granule flush latency
  is inside that bound); the last < granule cycles of a run are never flushed
  (phase-1 no-partial-flush contract — logged, not an error). Watchdog
  timeouts must absorb this: the demo config's
  `[watchdog] progress.timeout/xact.timeout = 1000000` exists for exactly
  this reason.
- **Determinism**: per-channel flit order/content is fully deterministic
  (FIFO order + in-order skids + echo retirement). With the anchored view and
  response slots, the *cycle-position* alignment is exact on any backend
  (loopback/Verilator proven by the rehearsal's `--strict-ab`; on hardware
  the same invariants hold by construction, since every visible event keys
  off DUT-cycle tags, never wall-clock pacing).
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
