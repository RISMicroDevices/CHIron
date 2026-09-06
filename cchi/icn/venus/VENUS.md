# Venus

Venus is a synthesizable SystemVerilog **coherency home** for the CCHI
("Compact CHI") protocol — a directory-based point of coherence between
multiple fully coherent (Type-1) upstream nodes and multiple AXI4 memory
channels. It is the reference DUT of the Cohestra V3 harness
(`-DCOHESTRA_V3_RTL=VENUS` / `VENUS_EXTMEM`).

- **Upstreams**: 1..8 CCHI Type-1 ports (`NUM_T1`, multi-core), 7 channels each.
- **Downstreams**: 1..4 AXI4 memory channels (`NUM_AXI`, multi-channel),
  line-hashed.
- **Parallelism**: 1..N in-flight transactions (`PARALLELISM` tracker slots,
  default 8).
- **Opcodes**: every CCHI opcode except atomics and exclusive-related ones —
  ReadNoSnp, ReadOnce, ReadShared (with promotion), ReadUnique, MakeUnique,
  WriteNoSnpPtl/Full, WriteUniquePtl/Full, CleanShared, CleanInvalid,
  MakeInvalid, StashShared/StashUnique, EVT Evict / WriteBackFull, and all four
  snoop opcodes. EvictBack/EvictClean are rejected as reserved.
- **Coherence state**: I/S/U directory with per-set tree-PLRU and victim
  back-invalidation; optional, configurable snoop filter (`SF_ENABLE`,
  `SF_ALLOW_OVER`, `SF_BROADCAST`).
- **VIPT TagAlias** (optional, off by default): per-holder virtual-address
  alias recording in the directory (`TAGALIAS_W`, 0..8, default 0); an
  upstream requesting a line it already holds under a *different* alias is
  back-invalidated with a targeted SnpToInvalid before the grant, while a
  matching alias is guaranteed snoop-free (`docs/PROTOCOL.md` §7.7).
- **Transaction-age arbitration** (optional, off by default): `AGE_MATRIX=1`
  switches every slot-based arbiter (directory/SF commands, per-channel AXI
  read/write, SNP/DnRSP/DnDAT emission) from fixed slot-index priority to
  oldest-first over one shared age matrix (oceanus `NCBTransactionAgeMatrix`
  idiom) — a structural no-starvation guarantee; the default 0 is the
  bit-identical legacy behavior (`docs/PROTOCOL.md` §7.8).
- **Structure**: per-line tracker slots (oceanus-TSHR-inspired), single-cycle
  admission with a same-address CAM, single-outstanding shared backends,
  per-cycle emission muxes. Directory-only by design: memory is always the
  clean backing copy.

## Documentation

| Document | Contents |
|---|---|
| `docs/PROTOCOL.md` | The CCHI protocol as implemented: channels, flit layouts, opcode tables, Resp encodings, DBID/TxnID lifecycle, ordering rules, Venus policies, deltas vs the CHIron C++ model. |
| `docs/TRANSACTIONS.md` | Detailed dataflow of every opcode transaction: message sequences, slot phase walks, directory/SF/AXI actions, state transitions, victim back-invalidation. |
| `docs/ARCHITECTURE.md` | Block diagram, tracker slot design, admission rule, backends, emission/ID steering, parameters, timing conventions, oceanus relationship. |

The RTL is commented to the same standard (file headers, per-phase comments
cross-referencing `TRANSACTIONS.md`, simulation assertions as executable
protocol invariants).

## Layout

- `venus.sv` — top level (frozen pin contract), admission, arbitration, steering.
- `venus_mshr.sv` — transaction tracker slot (all opcode flows).
- `venus_pkg.sv` — encodings, transaction kinds, flit structs, helpers.
- `venus_directory.sv`, `venus_snoop_filter.sv` — coherence state backends.
- `venus_axi_master.sv`, `venus_axi_map.sv` — AXI downstream.
- `venus_port.sv`, `venus_fifo.sv`, `venus_id_alloc.sv`, `venus_sram_box.sv`,
  `venus_sram_if.sv` — infrastructure.
- `tb/venus_tb.sv` — standalone directed testbench (21 scenarios, all opcode
  families; `VENUS_TB PASSED` on success).
- `tb/venus_v3_top.sv`, `tb/venus_v3_top_extmem.sv`, `tb/venus_axi_mem.sv` —
  Cohestra V3 simulation tops and the AXI slave model.

## Verification

- Directed: `verilator --binary --timing --top-module venus_tb ...` (see the
  source list in `../../cohestra/cohestra_v3/CMakeLists.txt`,
  `COHESTRA_V3_VENUS_SOURCES`).
- System: long FuzzARI runs under the `cohestra_v3` executable with the
  CacheLineDataMonitor enabled — see `../../cohestra/cohestra_v3/README.md`
  ("Fuzz sign-off") and `../../cohestra/cohestra_v3/config.fuzz.ini`.
