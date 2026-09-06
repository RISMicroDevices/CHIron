# Venus — CCHI Protocol Reference (as implemented)

This document defines the subset and interpretation of the CCHI ("Compact CHI") protocol
that Venus implements. It is derived from the CHIron C++ reference model in
`cchi/spec/cchi_protocol_encoding.hpp`, `cchi/spec/cchi_protocol_flits.hpp`, and the
`cchi/xact/` transaction layer, cross-checked against the Taurus upstream node model
(`cchi/icn/taurus/`). Where Venus's wire contract differs from the C++ defaults, the
delta is listed in §8.

Venus is a **HOME** component talking to **TYPE_1** (fully coherent) upstream nodes.

## 1. Topology and channels

CCHI is a two-tier point-to-point protocol between one downstream home and up to N
upstream nodes. There is no third tier, no DVM, no link-layer credits, and no Retry:
flow control is pure `valid`/`ready` backpressure on every channel.

Seven unidirectional channels (directions from the home's perspective):

| Channel | Direction | Carries |
|---|---|---|
| REQ   | upstream → home | requests (reads, writes, CMOs, stash) |
| EVT   | upstream → home | events (clean/dirty evictions) |
| UpRSP | upstream → home | CompAck, SnpResp |
| UpDAT | upstream → home | NonCopyBackWrData, CopyBackWrData, SnpRespData |
| SNP   | home → upstream | snoops |
| DnRSP | home → upstream | CompStash, Comp, DBIDResp, CompDBIDResp, CompCMO |
| DnDAT | home → upstream | CompData |

Each upstream is bound to a home port with an independent set of all seven channels.

## 2. Flit layouts (Venus wire configuration)

Widths below are the Venus wire configuration: TxnID 7, DBID 7, NodeID 5/5, Way 4,
Addr 48, Data 256 (2 beats per 64 B line).

The pin list is fixed at the architectural ceilings — 8 Type-1 port sets
(`cchi_t1p0..7_*`) and 4 AXI channel sets (`axi_m0..3_*`). `NUM_T1` (1..8) and
`NUM_AXI` (1..4) select how many are live; ports/channels beyond the live count
are tied off inside Venus (RX ready 0, TX valid 0) and must never see traffic
(a simulation assertion flags it). Field names and widths of a given port index
never change with the configuration.

### 2.1 EVT (74 bits)

| Field | Bits | Meaning |
|---|---|---|
| TxnID | 7 | per-source unique while in flight |
| SrcID | 5 | issuing upstream node |
| TgtID | 5 | target home |
| Opcode | 1 | §4.2 |
| Addr | 48 | byte address, line-aligned by convention |
| NS | 1 | non-secure (driven 0) |
| MemAttr | 1 | allocate attribute (compressed; ignored by Venus) |
| WayValid | 1 | upstream persisted-way hint (ignored) |
| Way | 4 | upstream way hint (ignored) |
| TraceTag | 1 | tracing (ignored, never echoed) |

### 2.2 REQ (97 bits)

| Field | Bits | Meaning |
|---|---|---|
| TxnID | 7 | per-source unique while in flight |
| SrcID / TgtID | 5 / 5 | requester / home |
| Opcode | 6 | §4.1 |
| Size | 3 | log2(bytes): 0..6 = 1 B..64 B; 7 reserved |
| Addr | 48 | byte address |
| TagAlias | 8 | VIPT virtual-address alias (§7.7); always carried on the wire, inert (driven 0) when `TAGALIAS_W=0` |
| NS | 1 | (driven 0) |
| Order | 2 | ordering — **no semantics defined; ignored** |
| MemAttr | 4 | attributes — **no encoding defined; ignored** |
| Excl | 1 | exclusive flag — **no exclusive opcodes exist; ignored** |
| ExpCompData / ExpCompStash | 1 | same wire bit: ExpCompData on reads, ExpCompStash on Stash* |
| WayValid / Way | 1 / 4 | hints (ignored) |
| TraceTag | 1 | (ignored) |

### 2.3 SNP (67 bits)

| Field | Bits | Meaning |
|---|---|---|
| TxnID | 7 | home-allocated snoop ID (shares the DBID ID-space per port) |
| SrcID / TgtID | 5 / 5 | home / snooped upstream |
| Opcode | 2 | §4.3 |
| Addr | 45 | line address << 3 (8 B granularity; Venus drives `{line,3'b000}`) |
| NS | 1 | |
| TraceTag | 1 | |

### 2.4 DnRSP (42 bits)

TxnID(7, echoes REQ/EVT), SrcID(5, home), TgtID(5, requester), DBID(7, §6),
Opcode(3, §4.4), RespErr(2, driven 0), Resp(3, §5), CBusy(3, driven 0),
WayValid/Way (driven 0), TraceTag (0).

### 2.5 UpRSP (25 bits)

TxnID(7 — **overloaded**: carries the DBID for CompAck, the SNP TxnID for SnpResp),
SrcID/TgtID(5/5), Opcode(1, §4.5), RespErr(2), Resp(3 — SnpResp final state, §5),
TraceTag(1).

### 2.6 DnDAT (270 bits)

TxnID(7, echoes REQ), SrcID/TgtID(5/5), DBID(7), Opcode(1, §4.6), RespErr(2, 0),
Resp(3, §5), DataSource(5, 0), CBusy(3, 0), DataID(1 — beat index at 256-bit data),
Data(8×32-bit words, unpacked on the wire), WayValid/Way (0), TraceTag(0).

### 2.7 UpDAT (315 bits)

TxnID(7 — carries the DBID for write data, the SNP TxnID for SnpRespData),
SrcID/TgtID(5/5), Opcode(2, §4.7), RespErr(2), Resp(3), DataID(1),
Data(8×32-bit), BE(32 — one bit per byte of the 32 B beat), TraceTag(1).

## 3. Cache states and Resp encodings

Upstream cache states (4-state model): **I**, **S** (shared-clean), **UC**
(unique-clean), **UD** (unique-dirty). UC→UD is a *silent* local transition: the home
learns about dirty data only via PD snoop responses or WriteBackFull.

Resp (3 bits) carried on DnRSP/UpRSP/DnDAT/UpDAT:

| Value | Name | Meaning |
|---|---|---|
| 000 | I | final state Invalid |
| 001 | SC | Shared-Clean |
| 010 | UC | Unique-Clean |
| 100 | I_PD | Invalid + Pass-Dirty (dirty data accompanies) |
| 101 | SC_PD | Shared + Pass-Dirty |
| 110 | UC_PD | Unique-Clean + Pass-Dirty |

`resp[2]` is the Pass-Dirty bit. Venus never emits `*_PD` on completions: all PD data
collected from snoops is written back to memory by the home itself.

Venus's directory tracks per line: **I** (absent) / **S** (sharers vector) /
**U** (unique at `owner`; may be silently dirty). The home cannot distinguish UC from
UD at a unique owner until a snoop or writeback reveals it.

## 4. Opcode tables

### 4.1 REQ (6 bits) — upstream → home

| Value | Name | Venus handling |
|---|---|---|
| 0x00 | StashShared | no-op hint; CompStash iff ExpCompStash=1 |
| 0x01 | StashUnique | no-op hint; CompStash iff ExpCompStash=1 |
| 0x02 | ReadNoSnp | AXI read → CompData×N |
| 0x03 | ReadOnce | peek (snoop dirty owner) → CompData×N, no allocation |
| 0x04 | ReadShared | allocate S, or promote to U (§7.4) → CompData×2 → CompAck |
| 0x08 | WriteNoSnpPtl | DBIDResp → NCBWrData×N → AXI write (wstrb) → Comp |
| 0x09 | WriteNoSnpFull | DBIDResp → NCBWrData×2 → AXI write → Comp |
| 0x0A | WriteUniquePtl | snoop peers (incl. requester) → DBIDResp → data×N → AXI write → Comp |
| 0x0B | WriteUniqueFull | snoop peers (incl. requester) → DBIDResp → data×2 → AXI write → Comp |
| 0x0C | CleanShared | clean dirty owner to PoC → CompCMO |
| 0x0D | CleanInvalid | invalidate all (incl. requester), write back dirty → CompCMO |
| 0x0E | MakeInvalid | invalidate all (incl. requester), drop dirty → CompCMO |
| 0x10 | ReadUnique | allocate U → CompData×2 (or Comp when ExpCompData=0 and the requester is a recorded holder) → CompAck |
| 0x12 | MakeUnique | invalidate peers → Comp → CompAck |
| 0x1E | EvictBack | **rejected** (broken in the CCHI Joint; reserved) |
| 0x1F | EvictClean | **rejected** (unmapped in the CCHI Joint; reserved) |
| 0x20–0x27 | AtomicLoad.* | **not supported** (dropped + sim warning) |
| 0x28–0x2F | AtomicStore.* | **not supported** (dropped + sim warning) |
| 0x30–0x31 | AtomicSwap, AtomicCompare | **not supported** (dropped + sim warning) |

### 4.2 EVT (1 bit) — upstream → home

| Value | Name | Venus handling |
|---|---|---|
| 0 | Evict | clean eviction notice → dir/SF remove → Comp |
| 1 | WriteBackFull | dirty eviction → CompDBIDResp → CopyBackWrData×2 → AXI write |

### 4.3 SNP (2 bits) — home → upstream

| Value | Name | Upstream behavior (Taurus-definitive) |
|---|---|---|
| 00 | SnpMakeInvalid | → I, always SnpResp_I; **dirty data is dropped** (never returns data) |
| 01 | SnpToInvalid | I/S/UC → I SnpResp_I; UD → I **SnpRespData_I_PD** |
| 10 | SnpToShared | I → I SnpResp_I; S/UC → S SnpResp_SC; UD → S **SnpRespData_SC_PD** |
| 11 | SnpToClean | I → I SnpResp_I; S → S SnpResp_SC; UC → UC SnpResp_UC; UD → UC **SnpRespData_UC_PD** |

Data is returned only from UD. SnpRespData is always whole-line: 2 beats, DataID 0/1,
BE all-ones, TxnID = snoop TxnID.

### 4.4 DnRSP (3 bits) — home → upstream

| Value | Name | Used for |
|---|---|---|
| 000 | CompStash | Stash* completion (only when ExpCompStash=1; never in the Taurus flow) |
| 001 | Comp | MakeUnique, RU-upgrade, Evict, writes (after data landed) |
| 010 | DBIDResp | data-buffer grant for writes |
| 011 | CompDBIDResp | WriteBackFull grant (Comp + DBID in one) |
| 100 | CompCMO | CMO completion |

### 4.5 UpRSP (1 bit)

| Value | Name | TxnID carries |
|---|---|---|
| 0 | CompAck | the DBID |
| 1 | SnpResp | the SNP TxnID |

### 4.6 DnDAT (1 bit)

| Value | Name |
|---|---|
| 0 | CompData |

### 4.7 UpDAT (2 bits)

| Value | Name | TxnID carries |
|---|---|---|
| 00 | NonCopyBackWrData | the DBID (WriteNoSnp*/WriteUnique*) |
| 10 | CopyBackWrData | the DBID (WriteBackFull); Resp always I_PD, BE all-ones |
| 11 | SnpRespData | the SNP TxnID |

## 5. Message-sequence rules (enforced by the CCHI Xaction layer)

1. **TxnID uniqueness**: (SrcID, TxnID) of REQ and EVT unique while in flight; freed at
   TxnID-complete. SNP TxnIDs unique per (home, target) while in flight.
2. **DBID lifecycle**: allocated by the home on the first DBID-bearing flit of a
   transaction (Comp / DBIDResp / CompDBIDResp / first CompData of an allocating read);
   constant and beat-consistent for the whole transaction; freed by CompAck
   (reads/dataless) or by the last write-data beat (writes/writebacks).
   (Home, requester, DBID) must be unique while in flight.
3. **CompAck**: legal after the *first* CompData beat (not necessarily the last) for
   reads, and after Comp for dataless. The home must hold the DBID and the transaction
   until it arrives.
4. **Comp vs CompData** are mutually exclusive per transaction; Comp is forbidden when
   ExpCompData=1; CompData is the only legal completion for ReadShared/ReadNoSnp/ReadOnce.
   ExpCompData is a hint from the requester: ExpCompData=0 allows Comp only while the
   requester is a recorded current holder; otherwise CompData is required (see §T4).
5. **Write data only after the DBID grant** (DBIDResp/CompDBIDResp); data TxnID = DBID,
   TgtID = grant's SrcID; beats carry distinct DataIDs covering the Size-derived mask.
6. **CompData beats of one transaction carry a constant Resp.**
7. **SnpResp xor SnpRespData** per snoop; SnpRespData always whole-line.
8. **M5 (Taurus hard rule)**: the home must never return CompData for a line before a
   colliding WriteBackFull's CopyBackWrData has been received and written. Venus
   enforces this structurally (§7.3 same-address serialization).
9. Stash completes silently when ExpCompStash=0 (zero responses); CompStash is sent only
   when ExpCompStash=1.
10. No Retry/credits anywhere; resource exhaustion is wire backpressure only.

## 6. Identifier spaces

Per (home, upstream port) there is **one** home-allocated 7-bit ID space shared by:
- SNP TxnIDs (freed when SnpResp/SnpRespData completes), and
- DBIDs (freed by CompAck or by the last write-data beat).

Because UpRSP.TxnID and UpDAT.TxnID carry either kind, no ID may be both a live snoop
ID and a live DBID. Venus keeps one allocator per port; per cycle at most one UpRSP-side
free (CompAck or SnpResp — same channel) and one UpDAT-side free (write-data last beat
or SnpRespData last beat) can occur, so two free ports per allocator suffice.

## 7. Venus policies

### 7.1 Snoop mask construction

```
mask = (directory-exact ∪ snoop-filter-presence [SF_ENABLE] ∪ all-ones [maybe_all | SF_BROADCAST])
if opcode is a read/MakeUnique: mask ∖= {requester}
if CMO (CleanShared/CleanInvalid/MakeInvalid) or WriteUnique* or victim eviction:
    mask includes the requester when present
```

Snoop-filter over-inclusion is harmless (absent upstreams answer SnpResp_I).
`SF_ALLOW_OVER=0` clips the mask to the directory-exact set; `SF_BROADCAST=1` forces
all-ones on every lookup.

### 7.2 Requester self-snoop

CleanShared, CleanInvalid, MakeInvalid, WriteUniqueFull and WriteUniquePtl snoop the
requester itself whenever it is recorded present. Taurus never self-invalidates on CMOs
and answers a self-snoop per the standard table even with its own REQ in flight, so
point-of-coherency maintenance holds without upstream cooperation.

### 7.3 Same-address serialization and EVT sharing

At most one **request** (REQ-kind) transaction per cache line is in flight
across all tracker slots. A new REQ to a busy line stalls at the port FIFO
head. An **EVT** (Evict / WriteBackFull) *may share* a line with an in-flight
REQ slot: Taurus parks a snoop answer while its own EVT to that line awaits
its first response, so blocking the EVT behind the REQ would deadlock the
port (the REQ waits for the snoop answer, the snoop answer waits for the EVT,
the EVT waits for the line). EVT transactions therefore always admit and run
concurrently; their directory/SF removals are PORT-mode (clear only the
evictor's own presence), which is order-safe against a same-line REQ slot
re-granting the line. The serialization extends to §T15 victim lines in both
directions (`vic_blk`/`o_vic_act`): a victim arm retries its DIR LOOKUP
(throttled by `VIC_RETRY_THROTTLE` so the fixed-priority directory arbiter
always sees request gaps) while the proposed victim line is another slot's
in-flight main line, and a REQ to a line with an armed victim sub-flow stalls
at admission — so no flow ever
plans on directory meta that a concurrent victim back-invalidation is about
to remove.

The M5 guarantee (§5.8) is then enforced at the data stage instead of by
blanket serialization: a read-flow slot (ReadShared/ReadUnique/ReadOnce) must
not issue its AXI read while another slot holds the same line with a
write-kind transaction whose AXI write has not yet landed (`o_wr_hold` /
`wb_blk`). The same hold covers the §T15 victim writeback: while a slot's
back-invalidation sub-flow is demoting the victim's holders (`PH_VIC_SNP`) or
writing the victim line back (`PH_VIC_WB`), memory for `s_vline` may be stale,
so any slot's AXI read of that line stalls (`o_vwb_hold`, matched on
`s_vline`) until the writeback lands. A reader whose snoops completed before
the victim demotions began is unaffected by construction: it either received
the dirty data as PD itself (and never fills from AXI) or saw the line clean.
A WB admitted after the read therefore still orders before the
read's data, and a read admitted after the WB stalls at admission (REQ rule)
until the WB retires. Directory under-inclusion is never possible: holder
bits are only ever set by the home's own GRANTs; concurrent eviction can
leave a *phantom* (over-inclusive) holder bit, which is harmless — spurious
snoops are always answered SnpResp_I.

### 7.4 ReadShared promotion

When the post-transaction present-set other than the requester is empty, ReadShared
completes CompData UC and allocates U{requester}; otherwise CompData SC and
S{present ∪ requester}. UC→UD being silent upstream, a later store at the sole tenant
needs no ReadUnique round-trip.

### 7.5 Size handling

Coherent allocating ops (ReadShared/ReadUnique/MakeUnique/WriteUniqueFull, all EVT) are
whole-line (B64) by definition. ReadNoSnp/ReadOnce/WriteNoSnp*/WriteUniquePtl honor
REQ.Size: beat count N = ceil(2^Size / 32 B) (1 or 2 at 256-bit data), AXI burst length
N, DataID starting at Addr[5].

### 7.6 AXI mapping

Line → AXI port by XOR-fold hash (`venus_axi_map`), NUM_AXI ∈ 1..4 (`AXI_MAX`
pin ceiling; non-power-of-two counts use modulo folding). Writes of dirty (PD)
data always go to memory: the home keeps memory the clean backing copy, so
directory S states and UC owner states are always backed.

### 7.7 VIPT TagAlias back-invalidation

TagAlias (REQ flit, §2.2) models VIPT virtual-address aliasing in upstream
caches: two VAs mapping to the same PA may carry different alias values. The
directory records, per entry, the alias each holder used (`aliases[p]`,
meaningful only while holder bit `p` is set). The feature is optional and
parameterized by `TAGALIAS_W` (0..8, default 0).

Detection happens at directory-lookup time (`s_alias_inv`: the LOOKUP alias
snapshot `rsp_aliases` compared against the request's `c_alias`) and applies
to the allocating-read kinds only — ReadShared, ReadUnique, MakeUnique. When
the requester is already
a recorded holder of the line but with a **different** recorded alias, the
requester is added to the snoop mask with the opcode for that target
overridden to SnpToInvalid, folded into the normal `PH_SNOOP` phase: the
standard SnpResp/SnpRespDataPD collection and the PD merge/writeback paths are
reused unchanged, so a dirty stale-alias copy both writes back to memory and
(for reads) serves the new request's CompData before the commit stores the new
alias (`cmd_aliases`, §T3/§T4/§T5).

A **matching** alias guarantees no self-snoop: the requester stays excluded
from the mask and the flow is unmodified. This covers the common same-alias
S→U store-after-load upgrade (ReadUnique from a recorded Shared holder),
which is never alias-snooped.

Design notes:

- **The snoop filter stores no alias state.** It is a lossy presence superset
  (`maybe_all` overflow, §7.1), so alias information there could never be
  authoritative; alias detection is directory-only.
- **The back-invalidation snoop is by PA.** SNP and EVT flits have no TagAlias
  field in the CCHI spec (only REQ does), so the SnpToInvalid invalidates
  whatever alias the upstream currently holds for the line.
- **WriteUnique and the CMO kinds need no alias path**: they already snoop the
  requester inherently (`kind_self_snoop`, §7.2).
- **An alias-invalidated requester is always owed data.** The back-invalidation
  snoops the requester's own copy away, so a ReadUnique completed under
  `s_alias_inv` always carries CompData — never dataless Comp — even when
  ExpCompData=0 (a snooped upstream copy makes data mandatory); if the stale
  copy was clean (no PD), the main line is refilled from AXI despite the
  directory hit.
- **`TAGALIAS_W=0` disables everything**: the wire field is inert (the pin is
  tied 0) and all alias storage and logic are generate-gated away.

### 7.8 Transaction-age arbitration

Venus's internal arbiters — directory command, snoop-filter command,
per-channel AXI read/write, and the SNP/DnRSP/DnDAT emission classes — are
parameter-switched by `AGE_MATRIX` (0/1, default 0):

- **`AGE_MATRIX=0` (default)**: fixed slot-index priority — the legacy
  behavior, bit-identical.
- **`AGE_MATRIX=1`**: transaction-age oldest-first via one shared
  `venus_age_matrix` over the PARALLELISM tracker slots (oceanus's
  NCBTransactionAgeMatrix idiom). A transaction's age is born at its slot
  admission (the newborn becomes the youngest); each arbiter combinationally
  selects the oldest requesting slot.

Oldest-first is a hard no-starvation guarantee: a waiting slot's age rank
only ever improves as older transactions retire, so every requester is
granted in bounded time — the fixed-priority starvation class behind the
§T15 victim-retry throttle is removed structurally (`VIC_RETRY_THROTTLE`
stays as a re-lookup rate limit in both modes). Admission across ports stays
round-robin in both modes. The switch is purely internal scheduling: nothing
on the wire changes — message sequences, Resp encodings, and all §5 rules are
identical, and an upstream can observe the policy only through timing.

## 8. Deltas vs the CHIron C++ model

| Topic | C++ model | Venus wire |
|---|---|---|
| DBID width | `FlitConfiguration` default 8 | **7** (DBID, SNP TxnID, UpRSP/UpDAT TxnID all 7) |
| RespErr/CBusy/DataSource | undefined | driven 0, ignored on receive |
| Order/MemAttr | undefined | driven 0, ignored |
| WayValid/Way hints | optional | pins present, driven 0, ignored on receive |
| EvictBack/EvictClean | broken/unmapped | rejected at admission (+ sim warning) |
| Atomics | declared, no Xaction | rejected at admission (+ sim warning) |
| Excl | unused bit | ignored |
| CompStash | legal when ExpCompStash=1 | implemented; never triggered by Taurus (always 0) |
| TagAlias (REQ) | optional field, width from `COHESTRA_TAGALIAS_WIDTH` (default 0) | always on the wire, fixed 8 bits; inert (driven 0) when `TAGALIAS_W=0`; Venus-side optional, both sides driven by the `COHESTRA_V3_TAGALIAS_WIDTH` knob |
