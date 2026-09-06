# Venus — Architecture

Venus is a synthesizable SystemVerilog **coherency home** for the CCHI protocol:
a directory-based, HN-F-style point of coherence sitting between up to 8 fully
coherent (Type-1) upstream nodes (multi-core) and up to 4 AXI4 downstream
memory channels. It implements every CCHI opcode except atomics and
exclusive-related ones (which do not exist in CCHI), with a configurable snoop
filter. Protocol details live in `docs/PROTOCOL.md`; per-opcode dataflows in
`docs/TRANSACTIONS.md`.

The architecture references the oceanus L2 (XSCache) idioms: per-address
serialization through one tracker per line, shadow-meta commit-once into
single-ported backends, event-driven per-transaction control instead of a
global recirculating pipeline, and assertions as executable documentation.

## 1. Block diagram

```
            upstream Type-1 nodes (x NUM_T1, pin ceiling T1_MAX=8)
                 | 7 CCHI channels per port (PROTOCOL §1)
        +--------v---------------------------------------------+
        | venus_port x NUM_T1: FIFO banks (rxevt/rxreq/rxrsp/  |
        | rxdat in; txsnp/txrsp/txdat out), occupancy-only     |
        | ready                                                |
        +--------+-----------------------------^---------------+
                 | heads                        | emission muxes
        +--------v---------------+   (<=1 SNP, <=1 DnRSP, <=1 DnDAT / cycle)
        | admission              |               |
        |  kind decode, drop     |        +------+------+
        |  same-address CAM,     |        | venus_id_alloc x NUM_T1
        |  round-robin over ports|        | (SNP TxnIDs + DBIDs,  |
        +--------v---------------+        |  shared per port)     |
                 | alloc (1/cycle)        +------^---------------+
        +--------v---------------+               | alloc/free
        | venus_mshr x PARALLELISM (tracker slots, ARCH §3)     |
        |  phase FSM + line buffer + snoop bookkeeping          |
        +--+------+------+--------+----------------+------------+
           |      |      |        |                ^
        dir cmd  sf cmd  AXI req  |            steering tables
        +--v------+--+   |        |            sn_slot / db_slot
        | venus_   | venus_snoop  |            (port,ID) -> slot
        | directory| _filter      |                ^
        | (I/S/U,  | (presence,   |   intake routing (UpRSP/UpDAT heads)
        |  PLRU)   |  maybe_all)  |
        +----------+--------------+
                 | single outstanding each, arbitrated per cycle
        +--------v-----------------+
        | venus_axi_master x AXI_N |  -> axi_m0..3 (multi-channel memory,
        +--------------------------+     pin ceiling AXI_MAX=4, line hash
```                                         from venus_axi_map)

## 2. The tracker slot (`venus_mshr`)

Each of the `PARALLELISM` (default 8) slots owns one transaction from admission
to retirement and registers its cache line in the same-address CAM for the
whole occupancy — oceanus's single-TSHR-per-address invariant (PROTOCOL §7.3).
A slot contains:

- **Request context**: kind, opcode, port, TxnID, line, Size, Addr[5],
  ExpCompData/ExpCompStash, AXI select.
- **Snapshots**: directory LOOKUP result (hit/state/sharers/owner/full/victim)
  and SF LOOKUP result (presence/maybe), captured once; all later decisions
  (snoop plan, commit image) are pure functions of the snapshots — the
  oceanus "read directory once, modify locally, commit once" idiom.
- **Line buffer**: 512-bit data + 64-bit byte strobe; collects AXI fills, PD
  snoop data, and write data (write wins over PD under BE).
- **Snoop bookkeeping**: pending/outstanding target bitmaps with the allocated
  per-target TxnIDs.
- **DBID register** for DBID-bearing flows.
- **Phase FSM** (`PH_*`, see TRANSACTIONS.md §T1..§T15): a coarse per-kind
  phase walk plus fine-grained collection bits (snoop bitmap, beat bitmap,
  early-CompAck latch `w_ack`).
- **Victim hazard interface**: `o_vline`/`o_vic_act` export the §T15 victim
  line (the fresh candidate on the arm-decision cycle) and the sub-flow's
  activity window; `vic_blk` comes back from the cross-slot victim hazard
  matrix in venus.sv and blocks the victim arm while the candidate is another
  slot's in-flight main line, forcing a *throttled* LOOKUP retry instead
  (`VIC_RETRY_THROTTLE` cycles between re-lookups — the fixed-priority
  directory arbiter would otherwise livelock against a directory-bound
  blocker). With `AGE_MATRIX=1` (§6) the directory arbiter becomes
  oldest-first, which structurally removes that fixed-priority starvation
  class; `VIC_RETRY_THROTTLE` stays unconditionally as a re-lookup rate
  limit. Symmetrically, `o_vic_act` feeds admission so a REQ to an armed
  victim line stalls until the sub-flow clears.

Backend requests (`dir_req`, `sf_req`, `axi_rd/wr_req`) are level-held until
the shared arbiter grants them; responses are routed back one-per-command.
Emission requests (`snp_req`, `dnrsp_req`, `dndat_req`) go to per-cycle muxes;
DBIDs and snoop TxnIDs are allocated atomically with the emission grant.

## 3. Admission and the same-address rule

Per cycle at most one head flit is admitted:

1. Per port, the EVT head outranks the REQ head (upstream hazard rules make
   mixed queues rare; EVT starvation is impossible because EVTs always
   complete without external input).
2. Kind decode: unsupported opcodes (reserved, EvictBack/EvictClean, atomics)
   are popped and dropped with a simulation warning (TRANSACTIONS §T12).
3. The candidate's line is checked against every busy slot's line (CAM);
   a hit stalls the candidate at the FIFO head.
4. Round-robin across ports picks the first admissible candidate; it enters
   the lowest free slot.

The rule keeps one REQ-kind transaction per line (single-writer discipline
for the directory) while allowing EVTs to share: Taurus parks a snoop answer
while its own EVT to that line awaits its first response, so an EVT must
always be able to complete that first response concurrently (PROTOCOL §7.3).
M5 is enforced by the read-flow write-hold (`o_wr_hold`/`wb_blk`): a read's
AXI read stalls behind any colliding not-yet-landed writeback. Directory/SF
evictions use PORT-mode removal so they commute with a same-line REQ slot's
commit; over-inclusive (phantom) holder bits are harmless by design.

## 4. Backends

### Directory (`venus_directory`)

One word per set packs all ways `{valid, tag(line), state I/S/U, sharers,
owner}` plus tree-PLRU (4-way; round-robin otherwise). With `TAGALIAS_W>0`
each entry additionally carries `aliases[NUM_T1] × TAGALIAS_W` bits recording
the VIPT TagAlias each holder used (PROTOCOL §7.7; `aliases[p]` is meaningful
only while holder bit `p` is set) — e.g. NUM_T1=4/TAGALIAS_W=8 adds 32 bits to
the 51-bit baseline entry, NUM_T1=8 adds 64. Commands are atomic
read-modify-write operations over the set word, one outstanding:

- `LOOKUP` — hit + meta, fullness, victim proposal (invalid-first else PLRU)
  with the victim's full meta for back-invalidation (§T15).
- `GRANT` — allocate/overwrite with the tracker's commit image; rejects with
  `full` when the set has no invalid way (never silently evicts).
- `REMOVE` — PORT mode (Evict: clear one sharer, drop when empty) or LINE
  mode (CMOs, WriteUnique, WriteBack, victim: drop the entry). LINE mode takes
  an optional match image (`cmd_men` + `{state,sharers,owner}`) for the §T15
  victim flow's atomic compare-and-remove: the drop no-ops when a racing GRANT
  changed the entry after the tracker's LOOKUP. The match image deliberately
  excludes `aliases` — identity is state/sharers/owner only, and no non-GRANT
  path mutates aliases.

### Snoop filter (`venus_snoop_filter`)

Optional (`SF_ENABLE`); per-line presence superset plus per-set `maybe_all`
for lossy replacement. It is alias-free by design: a lossy superset could
never hold authoritative per-holder alias state, so TagAlias detection lives
in the directory only (PROTOCOL §7.7). `LOOKUP` refines snoop masks; `UPDATE`
(exact rebuild, drops on empty) and `REMOVE` (per-port) are the commit-once
counterparts of directory updates. Policies `SF_ALLOW_OVER`/`SF_BROADCAST` compose the mask in
the slot (PROTOCOL §7.1). Over-inclusion only ever costs spurious snoops
(answered SnpResp_I), never correctness.

### AXI masters (`venus_axi_master`)

One per channel (`NUM_AXI` ∈ 1..4, `AXI_MAX` pin ceiling; line→channel hash in
`venus_axi_map`). One outstanding transaction: INCR bursts of 1–2 256-bit
beats, AxSIZE=32 B, burst length and start beat from the Size-aware flows,
full or partial write strobes. Reads win over writes when both are requested
while idle. A slot's channel select is the admitted line's hash, except during
the §T15 victim writeback (`PH_VIC_WB`), which follows the *victim* line's
hash — the transfer's address and channel must always belong to the same line.

### Storage (`venus_sram_box`, `venus_fifo`, `venus_id_alloc`)

Directory/SF storage is flop arrays (`STORAGE=0`) or a timed behavioral SRAM
model (`STORAGE=1`, LAT/INTERVAL/PERIOD+SLOT). FIFOs are occupancy-only-ready
flop FIFOs (the C++ tick model requires ready independent of same-cycle
valid). `venus_id_alloc` per port hands out the shared 7-bit snoop-TxnID/DBID
space (PROTOCOL §6) with two free ports — the protocol guarantees ≤1 UpRSP-side
and ≤1 UpDAT-side free per port per cycle.

## 5. Emission, steering, and IDs

- ≤1 SNP, ≤1 DnRSP, ≤1 DnDAT per cycle globally (fixed slot priority, or
  transaction-age oldest-first with `AGE_MATRIX=1` — PROTOCOL §7.8).
- ID allocation priority per port: SNP > DnRSP > DnDAT (matches the
  reference design).
- On each emission that allocates an ID, the arbiter records
  `(port, ID) → slot` in `sn_slot` (snoops) or `db_slot` (DBIDs).
- Returning UpRSP/UpDAT flits are decoded per port and steered by those
  tables: SnpResp/SnpRespData by SNP TxnID, CompAck/write data by DBID.
  Every UpRSP flit frees exactly one ID; every terminal UpDAT beat frees one.
- Evict retirement waits for the actual DnRSP wire fire (`txrsp_fire` +
  TxnID match), since the upstream may reuse the line immediately after
  seeing Comp.

## 6. Parameters

Geometry parameters are set at Verilator elaboration. In the Cohestra V3
builds they are driven by CMake cache variables which map to `-G` overrides on
the simulation top (`venus_v3_top` / `venus_v3_top_extmem`):

```sh
cmake .. -DCOHESTRA_V3_RTL=VENUS \
         -DCOHESTRA_V3_VENUS_NUM_T1=8 \
         -DCOHESTRA_V3_VENUS_NUM_AXI=4 \
         -DCOHESTRA_V3_VENUS_PARALLELISM=16
```

| Parameter | Default | Range | Meaning |
|---|---|---|---|
| `NUM_T1` | 4 | 1..8 (`T1_MAX`) | live Type-1 upstream ports; the pin list carries all 8 sets, ports ≥ NUM_T1 are tied off (sim error on traffic) |
| `NUM_AXI` | 1 | 1..4 (`AXI_MAX`) | live AXI channels; pins carry 4 sets, channels ≥ NUM_AXI tied off |
| `PARALLELISM` | 8 | ≥1 | tracker slots (max in-flight transactions) |
| `NODE_ID` | 16 | | home node ID |
| `INFLIGHT_SNP` | 4 | | per-port TX SNP FIFO depth |
| `Q_EVT/Q_REQ/Q_RSP/Q_DAT` | 2/2/8/16 | | per-port FIFO depths |
| `DIR_SETS/DIR_WAYS` | 1024/4 | | directory geometry |
| `SF_ENABLE` | 1 | | snoop filter present |
| `SF_ALLOW_OVER` | 1 | | allow over-inclusive masks |
| `SF_BROADCAST` | 0 | | always snoop all ports |
| `SF_SETS/SF_WAYS` | 256/4 | | snoop filter geometry |
| `TAGALIAS_W` | 0 | 0..8 | VIPT TagAlias width (PROTOCOL §7.7); 0 = feature off — storage/logic generate-gated away, pin tied 0; directory cost `NUM_T1 × TAGALIAS_W` bits/entry |
| `AGE_MATRIX` | 0 | 0/1 | slot-based arbitration policy (PROTOCOL §7.8): 0 = fixed slot-index priority (legacy, bit-identical), 1 = transaction-age oldest-first via `venus_age_matrix` |
| `STORAGE_DIR/STORAGE_SF` | 0 | | REG (0) or behavioral SRAM (1) |
| `SRAM_LAT/INTERVAL/PERIOD/SLOT` | 1/1/1/0 | | SRAM timing knobs |
| `AXI_ADDR_W/AXI_DATA_W/AXI_ID_W` | 48/256/4 | | AXI widths |

Scaling costs to keep in mind when raising the geometry knobs:

- **Arbitration is O(PARALLELISM) fixed-priority** combinational logic per
  backend (directory, snoop filter, per-channel AXI read/write) and per
  emission class; slot-index priority means slot 0 wins ties. With
  `AGE_MATRIX=1` arbitration becomes transaction-age oldest-first: one shared
  `venus_age_matrix` costs `PARALLELISM(PARALLELISM−1)/2` flops (28 at
  PARALLELISM=8) plus O(NSEL·PARALLELISM²) combinational select logic
  (NSEL = 5+2·NUM_AXI arbiter ports).
- **Steering tables** cost `NUM_T1 × ID_SPACE (128) × SLOT_W` bits each for
  `sn_slot` and `db_slot` (flops).
- **Sharer/presence vectors** in the directory, snoop filter, and every slot
  are `NUM_T1` bits wide; per-port ID allocators scale linearly with NUM_T1.
- **TagAlias storage** scales `NUM_T1 × TAGALIAS_W` bits per directory entry
  (NUM_T1=4/TAGALIAS_W=8: +32 bits on the 51-bit baseline; NUM_T1=8: +64); at
  the default `TAGALIAS_W=0` the alias storage and compare logic are
  generate-gated away.
- `PARALLELISM=1` is legal (fully serialized home) and useful for triage;
  `NUM_T1=1` degenerates to a single-requester home (no snoops ever).

The harness side needs no C++ changes: `V3CCHIInterface` concept-detects
Type-1 pin sets 0..7 and `V3AXISlaveInterface`/`V3AXIMonitorInterface` detect
AXI ports 0..7 at compile time. The runtime config must list 8 upstream node
IDs (`upstream.type1.nodeid = 0..7`); nodes not driven by a stimulator attach
but stay idle. Driving traffic on a port ≥ the build's `NUM_T1` raises a
simulation error in `venus`.

The `cchi_t1p{0..7}_rxreq_bits_TagAlias` pins are part of the frozen V3 pin
contract: always present, fixed 8 bits per port (the WayValid/Way precedent).
The Cohestra wrapper drives them from `flit.TagAlias` when the C++ side is
built with `COHESTRA_TAGALIAS_WIDTH>0`, constant 0 otherwise; inside Venus the
mapped field is zeroed and all alias storage/logic is generate-gated away when
`TAGALIAS_W=0` (a simulation assertion flags a nonzero pin in that
configuration).

## 7. Timing and reset conventions

- Single `clock`; `reset` is high-active, asynchronously asserted and
  synchronously deasserted (`always_ff @(posedge clock or posedge reset)`).
- RX-channel `ready` is occupancy-only — never a combinational function of
  same-cycle `valid`/payload (Cohestra V3 tick-model requirement).
- All shared resources are single-outstanding with req/gnt/resp handshakes;
  arbitration is fixed slot-index priority (fairness comes from slot
  symmetry; admission across ports is round-robin). With `AGE_MATRIX=1`
  arbitration is transaction-age oldest-first instead (birth = slot
  admission), a hard no-starvation guarantee (PROTOCOL §7.8); admission
  across ports stays round-robin either way.

## 8. Relationship to oceanus (XSCache)

Mimicked: one tracker per address (TSHR→slot), per-channel FIFO banks,
shadow-meta commit-once to single-ported backends, PLRU + victim
back-invalidation, event-driven per-transaction control, assertion density,
and — with `AGE_MATRIX=1` — the transaction age matrix
(NCBTransactionAgeMatrix → `venus_age_matrix`, oldest-first arbitration).
Deliberately not copied: the recirculating main pipe (openLLC/coupledL2), the
downstream-CHI RN-F half of oceanus (Venus's downstream is plain AXI), the
loopback-EvictBack protocol (victim eviction is an in-slot sub-flow instead —
Venus has no second TSHR to hand it to, and the CCHI EvictBack opcode is
broken upstream anyway), and oceanus's data array (Venus is directory-only by
decision: memory is always the clean backing copy).
