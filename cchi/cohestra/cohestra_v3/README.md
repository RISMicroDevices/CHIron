# cohestra_v3

Verilator harness for a CCHI Type-1 home. The C++ executable (`executable.cpp`) binds `V3CCHIInterface` to the Verilated top's `cchi_t1p{0..7}_*` pins and drives `clock` / high-active `reset`.

Configure from this directory (or its `build/` subdirectory). Do **not** run `cmake` from the CHIron repository root — that hits the wrong cache.

## CMake cache variables

| Variable | Default | Meaning |
|---|---|---|
| `COHESTRA_V3_RTL` | `DUMMY` | `DUMMY`, `VENUS`, `VENUS_EXTMEM`, or `CUSTOM` |
| `COHESTRA_V3_TOP_MODULE` | (empty) | Required for `CUSTOM`. Ignored for presets. |
| `COHESTRA_V3_RTL_SOURCES` | (empty) | Semicolon-separated SystemVerilog/Verilog files for `CUSTOM`. |
| `COHESTRA_V3_PREFIX` | (empty) | Optional Verilator `PREFIX`. Defaults: `Vtop_dummy` / `Vvenus` / `Vvenus_v3_top_extmem` / `V<TOP_MODULE>`. |
| `COHESTRA_V3_ENABLE_WALL` | `OFF` | When `ON`, compile with `-Wall -Wextra -Wpedantic` (or `/W4 /permissive-` on MSVC). |
| `COHESTRA_V3_WAVEFORM_FORMAT` | `FST` | Trace format compiled in: `FST` or `VCD`. Runtime `waveform.type` must match. |
| `COHESTRA_V3_VENUS_NUM_T1` | `4` | Venus Type-1 upstream ports, 1..8 (Venus presets only). |
| `COHESTRA_V3_VENUS_NUM_AXI` | `1` | Venus AXI downstream channels, 1..4 (Venus presets only). |
| `COHESTRA_V3_VENUS_PARALLELISM` | `8` | Venus MSHR tracker slots, ≥1 (Venus presets only). |
| `COHESTRA_V3_TAGALIAS_WIDTH` | `0` | VIPT TagAlias width, 0..8 (Venus presets only); drives both the Venus `-GTAGALIAS_W` override and the C++ `COHESTRA_TAGALIAS_WIDTH` compile define. 0 = feature off. |
| `COHESTRA_V3_AGE_MATRIX` | `0` | Venus slot arbitration (Venus presets only): 0 = fixed slot-index priority (legacy, bit-identical), 1 = transaction-age oldest-first (`venus_age_matrix`). Maps to `-GAGE_MATRIX`; see `../../icn/venus/docs/PROTOCOL.md` §7.8. |
| `COHESTRA_V3_THREADS` | `0` | Verilator worker threads (`--threads N`): 0 = legacy single-threaded model; N ≥ 1 builds the mtask-partitioned model with N workers. Also emitted as the `COHESTRA_V3_THREADS` compile define. |
| `COHESTRA_V3_PROF_EXEC` | `0` | Verilator execution profiling (`--prof-exec`): 1 emits per-mtask timing records at runtime (`profile_exec.dat`, viewable with `verilator_gantt`). Measurement only. |
| `COHESTRA_V3_OPT_LEVEL` | `3` | Optimization level for the harness + verilated model (`-O<n>`; empty = toolchain/build-type default; `/O2` on MSVC). Verilator workloads at `-O0` lose ~9-17x vs `-O3`, so 3 is the default. |

A custom (or Venus) top that this harness will clock must expose:

- `clock` and high-active `reset`
- the flattened `cchi_t1p{N}_*` Decoupled pin names used by `V3CCHIInterface` / `top_dummy.sv`

## Command-line arguments

Usage: `cohestra_v3 [--config <path>] [--cycles <n>] [--log-file[=<path>]] [--no-log-file] [--help]`

| Argument | Default | Meaning |
|---|---|---|
| `-h`, `--help` | — | Print the usage line and exit (code 0). |
| `--config <path>`, `--config=<path>`, `-c <path>` | (none) | INI configuration file. Optional — without it the executable runs on compiled-in defaults, but FuzzARI requires one. Wrapped double quotes are stripped. |
| `--cycles <n>`, `-n <n>` | `200000` | Cycle limit; the only stop condition. `[root] maximum_cycles` is not read by this executable. |
| `--log-file` | on | Log to a file (default). Without a path the file is `<exe-name>_<yyyymmdd>_<hhmmss>.log` in the working directory. |
| `--log-file=<path>` | — | Log to an explicit file path. |
| `--no-log-file` | — | Disable file logging (console only). |
| `--` | — | End of options; later tokens are positional. Positionals are accepted and currently ignored. |

Exit codes: `0` ran to the cycle limit (or `--help`); `1` argument or startup failure (unknown/invalid argument, unreadable or rejected configuration); `2` verification failure (monitor mismatch, watchdog timeout, Taurus transaction denial).

```bash
# Dummy smoke: reference config, 10k cycles, console-only logging
./build/cohestra_v3 --config config.ini --cycles 10000 --no-log-file

# Long Venus fuzz with an explicit log file
./build-8p4axi/cohestra_v3 -c config.fuzz8p.ini -n 200000000 --log-file=fuzz8p.log

# Multithreaded build and run
cmake -S . -B build-mt -DCOHESTRA_V3_RTL=VENUS_EXTMEM -DCOHESTRA_V3_THREADS=4
cmake --build build-mt -j$(nproc)
./build-mt/cohestra_v3 --config config.fuzz.ini --cycles 6000000
```

## Default: dummy top

Combinational loopback (`top_dummy.sv`). `clock` and `reset` are unused ports so the harness C++ compiles.

```bash
cd cchi/cohestra/cohestra_v3
cmake -S . -B build
cmake --build build -j$(nproc)
./build/cohestra_v3 --config config.ini --cycles 10000
```

From an existing `build/` directory:

```bash
cd cchi/cohestra/cohestra_v3/build
cmake ..
cmake --build . -j$(nproc)
./cohestra_v3 --config ../config.ini --cycles 10000
```

Dummy is not a coherent home. A smoke run only checks that the binary constructs, resets, and ticks.

## Venus

Synthesizable CCHI Type-1 coherency home (directory + snoop filter, all CCHI
opcodes except atomics; see `../../icn/venus/docs/PROTOCOL.md`). The `VENUS` preset wraps
it with the internal Verilog AXI slave (`../../icn/venus/tb/venus_v3_top.sv`); the
`VENUS_EXTMEM` preset exposes the AXI masters on the boundary
(`../../icn/venus/tb/venus_v3_top_extmem.sv`) to be served by the C++ `AXIMemory`.

```bash
cd cchi/cohestra/cohestra_v3/build
cmake .. -DCOHESTRA_V3_RTL=VENUS
# or: cmake .. -DCOHESTRA_V3_RTL=VENUS_EXTMEM
cmake --build . -j$(nproc)
./cohestra_v3 --config ../config.ini --cycles 10000
```

`--config` is required for FuzzARI. `--cycles` is the only stop condition.

### Venus geometry sweeps

The Venus geometry is compile-time (`-G` overrides on the top module) via the
`COHESTRA_V3_VENUS_*` cache variables. Use one build directory per
configuration:

```bash
# 8 upstreams, 4 AXI channels, 16 tracker slots
cmake -S . -B build-8p4axi -DCOHESTRA_V3_RTL=VENUS_EXTMEM \
    -DCOHESTRA_V3_VENUS_NUM_T1=8 -DCOHESTRA_V3_VENUS_NUM_AXI=4 \
    -DCOHESTRA_V3_VENUS_PARALLELISM=16
cmake --build build-8p4axi -j$(nproc)
./build-8p4axi/cohestra_v3 --config config.fuzz8p.ini --cycles 6000000
```

The config's stimulator upstream list selects the *active* nodes and must stay
below the build's `NUM_T1` (`config.fuzz.ini` drives 0..3, `config.fuzz2p.ini`
0..1, `config.fuzz8p.ini` 0..7). `upstream.type1.nodeid` always lists all 8
pin-set node IDs; undriven nodes attach but stay idle. Driving traffic on a
port ≥ `NUM_T1` raises a simulation error in the DUT. See
`../../icn/venus/docs/ARCHITECTURE.md` §6 for ranges and scaling costs.

The CCHI interface FIFOs every DUT-facing TX channel (EVT/REQ/RSP/DAT) with a
default depth of 4, decoupling upstream model retirement from the DUT's ready
timing. The depths are runtime-configurable per channel (applied to all
Type-1 ports) via `upstream.type1.buffer.{evt,req,rsp,dat}` in `[root]`.
Setting a depth to 0 frees the FIFO and reverts that channel to the unbuffered
direct-drive path, which carries a known stale-ready hazard (see the
`V3CCHIInterface` constructor comment in `interface.hpp`) — not recommended.

## Fuzz sign-off (long ARI fuzzing + data monitor)

`config.fuzz.ini` drives 4 Taurus Type-1 upstreams with FuzzARI and enables the
CacheLineDataMonitor, which scoreboards expected cache-line images from the
upstream nodes' CacheLine events and re-verifies every data-carrying grant,
dataless grant, and load. Any mismatch fails the run (exit code 2, independent
of `fail.on.mismatch`); any Taurus transaction denial also fails the run.

### Taurus reply randomization

Optional `[root]` overrides control randomized/delayed outbound Taurus replies
without changing TXREQ/TXEVT requests or Cohestra's cross-channel polling
order; Cohestra enables reply ranking and 0..8-tick delays by default. The scope is TXRSP (CompAck/SnpResp) and TXDAT
(CopyBackWrData/SnpRespData) only. See
`../../icn/taurus/docs/USER_GUIDE.md` §7.2 for selection and eligibility
details.

| Key | Default | Meaning |
|---|---|---|
| `upstream.type1.random.seed` | `0` | base reply seed; each node's seed is base + its zero-based upstream index |
| `upstream.type1.random.reply.order.enable` | `1` | `1` selects pending replies by a stable seed/action-age rank instead of age order |
| `upstream.type1.random.reply.delay.min` | `0` | inclusive minimum per-action reply delay |
| `upstream.type1.random.reply.delay.max` | `8` | inclusive maximum delay; `0` means immediate eligibility |

Cohestra enables these defaults even though Taurus's own default configuration
is deterministic and immediate. Ordering and delay are independent. Ranks and
delays are deterministic for a given node seed and action-age schedule, with no
mutable RNG in Taurus Peek/Pop. Equal ranks tie-break by action age and line
key. Per-line priorities remain SnpResp before CompAck and SNP DAT0/DAT1 before
EVT DAT0/DAT1. Normal channel-pending beats emit DAT0 before DAT1 (the existing
listener-forced data-hazard path is the documented exception), and both beats of
a reply share rank and ready time. Set `order.enable=0` and `delay.max=0` for
legacy age ordering and immediate replies. A delay range with min > max fails
initialization.

Delays use raw `Tick(time)` units. V3 and VCS advance time by **2 ticks per
clock cycle** (half-cycle ticks), while Earth uses **1 tick per cycle**. A reply
pended at time `T` is eligible at `T+D` (saturating), so a fixed `D=2` delay is
one V3/VCS cycle or two Earth cycles. `min=max` selects a fixed delay; a ranged
delay is sampled once per action from the inclusive interval. An unready reply
skips its line for that polling call and fires no Taurus `PreChannelChosen`
event. Reply delay does not postpone grant futures or change hazard/protocol
state at pend time.

Current observer caveat: a delayed dirty SnpRespData response can be legally
overtaken by a same-line request after TickSNP fixed its Resp/data. The
observer-only `CacheStateMap` may then emit a false
`XACT_DENIED_STATE_RESP_SNPRESPDATA` warning because it evaluates the narrowed
later state instead of response-determination history. This does not indicate a
CacheLineDataMonitor mismatch; the checker integration needs that history fix.
A randomized OpenLLC smoke exposed this warning while still reaching FINISHED
with zero mismatches and zero watchdog triggers.

The feature is on by default in Cohestra (`order.enable=1`, delay `0..8`). To
customize it, set a seed and/or bounds in `[root]`:

```ini
upstream.type1.random.seed                  = 1000
upstream.type1.random.reply.delay.min       = 2
upstream.type1.random.reply.delay.max       = 8
```

For a deterministic legacy replay, opt out explicitly:

```ini
upstream.type1.random.reply.order.enable    = 0
upstream.type1.random.reply.delay.max       = 0
```

Then run, for example:

```bash
./build/cohestra_v3 --config config.fuzz.random.ini --cycles 1000000
```

An inclusive 2..8 delay range is 1..4 cycles in V3/VCS (or 2..8 cycles in
Earth). The shipped `config.fuzz.ini` keeps only commented-out examples.

With `fuzz.ari.cmo.enable = 1` (default), FuzzARI emits real CBO transactions:
CBOClean / CBOFlush / CBOInval (CCHI `CleanShared` / `CleanInvalid` /
`MakeInvalid` → `CompCMO`), exercising the home's CMO flows against live
multi-upstream traffic. The CMO class rate is `1/fuzz.ari.cmo.ratio` (default
1/8) and the MakeInvalid share of CMOs is `1/fuzz.ari.cmo.inval.ratio`
(default 1/4, the rest split between CBOClean/CBOFlush); both are kept low on
purpose — CleanInvalid/MakeInvalid remove the home's directory entry, so a
high CMO rate would keep the directory from ever filling and starve the
victim back-invalidation flow.

Each driven source can be switched off individually:
`fuzz.ari.load.enable` (ReadShared), `fuzz.ari.store.enable` (ReadUnique),
`fuzz.ari.storeline.enable` (MakeUnique), and per-CMO
`fuzz.ari.cmo.{clean,flush,inval}.enable` — all default 1. A class (memory /
CMO) is active iff its class switch (`fuzz.ari.memory.enable` /
`fuzz.ari.cmo.enable`) is on AND at least one of its sources is enabled;
disabling everything makes the stimulator idle. The selection draws are kept
in the legacy RNG order, so the all-enabled default replays bit-identically
with pre-switch seeds.

Granted store acquisitions (DoStore or DoStoreLine) are followed by a real
full-line data commit into the upstream node's local CacheLine (deferred out
of the node's completion callback into the stimulator's Tick), so every
fuzzed store makes the whole line dirty with fresh random data.
Dirty data then reaches memory only through the DUT: CBOClean/CBOFlush
writebacks and victim back-invalidation writebacks — the AXI monitor's write
count is the direct evidence (it stays 0 if no dirty data ever flows out).
CleanShared/CleanInvalid write dirty data back, so the monitor's expectations
stay valid and verify the CMO writeback path. MakeInvalid architecturally
discards dirty data without a writeback, so on each MakeInvalid completion
the monitor drops the line's expectation (reported as "CMO invalidations";
the image is re-learned on the next fill). The stimulator's finish line
reports per-class counts as
`Issued N (CMO n: clean c, flush f, inval i), denied M (CMO m), stores
committed s, dropped d`. The cohestra_earth model does not service CMO
requests, so the earth configs keep `fuzz.ari.cmo.enable = 0`.

With a build at `COHESTRA_V3_TAGALIAS_WIDTH>0`,
`fuzz.ari.tagalias.permillage` (default 0) enables the FuzzARI TagAlias (VIPT
aliasing) model: each upstream keeps a persistent per-line alias map — a
line's alias is assigned on first access and kept, and a remap to a different
alias (a Venus back-invalidation trigger, see `../../icn/venus/docs/PROTOCOL.md` §7.7)
happens with probability permillage/1000 per access, so aliasing stays rare
and same-alias sequences (incl. S→U store-after-load upgrades) dominate.
`fuzz.ari.range.alias` sizes the alias space; `tagalias.permillage = 0`
(default) or `range.alias = 1` disables aliasing entirely (bit-identical
stimulus to a pre-TagAlias tree). `config.fuzz_ta.ini` is the feature-on
example config (`tagalias.permillage = 10`).

`fuzz.ari.tagalias.read.permillage` (default 0) adds targeted
TagAlias-conflicting reads: with probability permillage/1000 per tick the
fuzzer picks a line the upstream already holds in **Shared** state and re-reads
it under a *different* alias. A real VIPT L1 misses in the second set on such
an access, so the Taurus model lets the `ReadShared` reach the wire instead of
serving the local hit (its `DoLoad` alias-miss path, restricted to Shared-held
lines because `ReadShared` is not a legal issue from Unique), and the DUT must
snoop the old-alias copy before granting — the nested alias snoop. The
CacheLineDataMonitor rejects the failure mode outright: a grant landing while
the node still holds the same PA under a different alias (no intervening
invalidation) means the L1 physically holds two cache lines with the same PA
but different TagAlias; the monitor logs the conflict and fails the instance
(gated by `monitor.fail.on.mismatch`, default 1, and reported as "TagAlias
conflicts" in the monitor report). Both features compile out entirely in
width-0 builds and keep legacy RNG sequences when their knob is 0.

```bash
cd cchi/cohestra/cohestra_v3/build
cmake .. -DCOHESTRA_V3_RTL=VENUS_EXTMEM
cmake --build . -j$(nproc)
./cohestra_v3 --config ../config.fuzz.ini --cycles 1000000        # smoke
./cohestra_v3 --config ../config.fuzz.ini --cycles 200000000      # long fuzz
```

Monitor keys (`[monitor]` section): `enable` (default 1), `fail.on.mismatch`
(1), `assume.zero.init` (0; set 1 — the AXI backing is zero-initialized),
`trace` (0), `history.length` (4; per-line provenance depth reported on
mismatch). Note `[root] maximum_cycles` is not read by this executable; the
cycle limit is CLI-only.

Reset keys (`[root]` section): `reset_cycles` (default 100), `reset_repeat`
(default 1), and `reset_preamble` (default 1024). The reset stage first runs
`reset_preamble` cycles with reset **deasserted**, then emits `reset_repeat`
high-active pulses, each `reset_cycles` cycles reset-active followed by
`reset_cycles` cycles reset-inactive. Some targets hold power-on state behind
asynchronous-reset flops that only the reset *assertion edge* initializes in a
2-state (Verilator) model — the preamble guarantees that edge exists (starting
the simulation with reset already asserted does not). Set `reset_preamble = 0`
only for targets that do not need it. Both keys are honored from the INI
(`reset_cycles` was silently fixed at the compiled default 100 before).
`reset_repeat = 0` is invalid and clamped to 1 with a warning. Note the
default single pulse also ends with its inactive phase, so post-reset log
timestamps sit 200 ticks (100 cycles) later than pre-feature runs.

Watchdog keys (`[watchdog]` section):

| key | default | meaning |
|---|---|---|
| `enable` | 1 | master switch (0 = no listeners, no per-cycle work) |
| `progress.timeout` | 200000 | livelock tier: max time units without any transaction progress event while transactions are outstanding (0 disables the tier) |
| `xact.timeout` | 100000 | deadlock tier: max time units a DUT-outstanding transaction may wait for its first response (0 disables the tier) |
| `fail.on.timeout` | 1 | 1 = fail the instance on trigger (exit code 2); 0 = log each trigger once and continue |
| `dump.limit` | 8 | max outstanding transactions dumped per trigger |

The transaction watchdog supervises every transaction from the moment it
becomes DUT-outstanding until its first response, in two tiers: the
livelock tier fires when the whole system goes silent (no transaction
completion/response event for `progress.timeout` time units while
transactions are outstanding), and the deadlock tier fires when the oldest
outstanding transaction exceeds `xact.timeout`, naming the stuck
transaction (upstream, class, TxnID, opcode, address, age). Time values
use the harness `time` unit (2 per clock, the same scale as `--cycles`).
The default timeouts sit far above any legitimate stall — tune them down
only for triage. The `cohestra_earth` executable is not wired with the
watchdog (out of scope), and a transaction wedged upstream before its flit
is channel-accepted (never DUT-outstanding) is only bounded by the global
tier while other work is outstanding.

## Custom Verilog

```bash
cd cchi/cohestra/cohestra_v3/build
cmake .. \
    -DCOHESTRA_V3_RTL=CUSTOM \
    -DCOHESTRA_V3_TOP_MODULE=my_top \
    -DCOHESTRA_V3_RTL_SOURCES="/abs/path/a.sv;/abs/path/b.sv"
cmake --build . -j$(nproc)
./cohestra_v3 --config ../config.ini --cycles 10000
```

Optional prefix override: `-DCOHESTRA_V3_PREFIX=Vmy_top`.

## Warnings

`-Wall` is off by default. To restore the previous warning set:

```bash
cmake .. -DCOHESTRA_V3_ENABLE_WALL=ON
```

## Switching RTL after a previous configure

Changing `COHESTRA_V3_RTL` (or the Verilator prefix) in an existing `build/` is a reconfigure of that directory:

```bash
cd cchi/cohestra/cohestra_v3/build
cmake .. -DCOHESTRA_V3_RTL=DUMMY
# or
cmake .. -DCOHESTRA_V3_RTL=VENUS
cmake --build . -j$(nproc)
```

If the Verilator prefix changes and the build is stale, remove `build/` and configure again from `cchi/cohestra/cohestra_v3`.
