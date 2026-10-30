# Taurus — Architecture

Taurus is a behavioral C-CHI **Type-1 upstream node**: a synthetic CPU plus a
64-byte-line cache (states I/S/UC/UD) used as the stimulus endpoint of the
Cohestra verification harness. It is header-only C++20 — a single template,
`CCHI::Taurus::UpstreamNode<config>` with `template<FlitConfigurationConcept
config>`, in `cchi_taurus_component.hpp` (4781 lines), with the event classes
in `cchi_taurus_component_events.hpp`. Per-transaction message flows live in
[TRANSACTIONS.md](TRANSACTIONS.md), the hazard rules in
[HAZARDS.md](HAZARDS.md), API usage in [USER_GUIDE.md](USER_GUIDE.md), and the
documentation conventions in
[DOCUMENTATION_GUIDE.md](DOCUMENTATION_GUIDE.md).

All `file:line` citations below were taken from the current sources; the
component file is cited bare (`cchi_taurus_component.hpp:1081`), other files
relative to this directory.

## 1. Overview

Taurus sits at the top of the C-CHI C++ stack and below the Cohestra harness:

```
 cchi/spec       flit layouts, opcodes, sizes, FlitConfiguration
 cchi/xact       transaction FSMs: Joint, Xaction* classes, CacheStateMap
 cchi/icn        node models -- taurus (behavioral upstream, this model)
 cchi/cohestra   verification harness: Instance, stimulators, DUT binding
```

```
              stimulator / test code (Do* calls, CacheLine Load/Store)
                              |
                    +---------v----------+   TXEVT / TXREQ / TXRSP / TXDAT
                    |  Taurus            | ----------------------->
                    |  UpstreamNode      |                         downstream
                    |  (CPU + cache)     | <-----------------------  (Cohestra
                    +---------^----------+   RXSNP / RXRSP / RXDAT    DUT binding)
                              |
                    Cohestra Instance::Tick drives both sides
                    (../../cohestra/cohestra_instance.cpp:224-295)
```

What the node implements:

- **Upstream-initiated reads**: `DoLoad` → ReadShared, `DoStore` → ReadUnique,
  `DoStoreLine` → MakeUnique, all allocating 64 B line fills.
- **Evictions**: `DoEvict` → Evict (clean) or WriteBackFull (dirty), with
  CopyBackWrData beats built after DBIDResp/CompDBIDResp.
- **Prefetches**: `DoPrefetchLoad`/`DoPrefetchStore` → StashShared/StashUnique
  out of a FIFO queue.
- **Cache-maintenance ops**: `DoCBOClean`/`DoCBOFlush`/`DoCBOInval` →
  CleanShared/CleanInvalid/MakeInvalid out of a FIFO queue plus a completion
  tracker.
- **Snoop servicing**: SnpMakeInvalid/SnpToInvalid/SnpToShared/SnpToClean with
  SnpResp or two-beat SnpRespData replies, including the hazard interactions
  of a coherent cache.
- **Responses consumed**: Comp, DBIDResp, CompDBIDResp, CompCMO (RXRSP) and
  CompData (RXDAT); **responses emitted**: SnpResp, CompAck (TXRSP),
  SnpRespData, CopyBackWrData (TXDAT).

Declared but stubbed APIs (return `nullptr`/`false` today): `DoEvictSilently`
(cchi_taurus_component.hpp:1544-1550), `DoLoadThrough`/`DoStoreThrough`
(:1650-1664), `DoNonCacheableRead`/`DoNonCacheableWrite` (:1666-1680). The
constructor flags `enableSilentEviction` and `enableStrictInitialState`
(:454-455) are stored but never read inside the component — they gate no
behavior today.

Everything is 64-byte-line granular: any PA handed to a `Do*`/query API is
normalized to its line base — the same `CacheLine`, hazard state and wire flit
(`Addr`, always `Size=B64`) serve every address inside the line (contract
comment :499-503, `LineBase()` :801-805, `LineKey()` :807-811).

## 2. Block diagram

```
   DoLoad/DoStore/DoStoreLine   DoEvict    DoPrefetch*      DoCBO*     PushRXSNP
          |                       |            |               |            |
          v                       v            v               v            v
   +---------------------------------------------------------------------------+
   | action entry: LineBase normalize, hit fast path, xactionLimit* checks,    |
   | AllocateTxnID (not for snoops), NextEmissionAge stamp                     |
   +-------------------+----------------------------+------------------+
                       | per line (cacheable map)   | per class (FIFO) |
            +----------v-----------+        +-------v-------+ +--------v------+
            | CacheLine            |        | prefetchQueue | | cmoQueue      |
            |  data[8], state      |        +-------+-------+ +--------+------+
            |  hazard-pending slots|                | front          | front
            |  channel-pending slots                |                |
            |  activeEVT/SNP/REQ + futures          |                |
            +----------+-----------+                |                |
                       | Pend*Channel* helpers      |                |
            +----------v---------------------------v----------------v------+
            | aged deques of (age, LineKey), sorted by action-start age:   |
            | agedTXEVT, agedTXREQ (per-line only), agedTXRSP, agedTXDAT   |
            +----------+---------------------------------------------------+
                       | PeekTX*/PopTX*  (default age order; optional seeded
                       |  TXRSP/TXDAT ranks + readiness; per-line priority)
            +----------v---------------------------------------------------+
            | Xact::Joint<config>   -->   Xact::CacheStateMap<config>      |
            | (protocol FSMs; one         (shadow state checker, observer- |
            |  Xaction per transaction)    only, OnCacheStateMapDenied*)   |
            +----------+---------------------------------------------------+
                       | whole flit structs by value (std::optional)
                       v
              Cohestra harness / DUT channel queues

   Tick(time): TickEVT -> TickSNP -> TickREQ -> reap idle Invalid lines
   (hazard re-detection + slot releases, response pends, grant/evict future
    fires, TxnID frees)
```

The `EventHub` taps every stage drawn above: each arrow between boxes fires
Pre/Post event pairs (§9).

## 3. The cache-line model

### States

Four states (`cchi_taurus_state.hpp:33-40`): `Invalid`, `Shared`,
`UniqueClean`, `UniqueDirty` — the CHI I/S/UC/UD subset. `CacheStateEnum` is a
pointer-to-constexpr-descriptor type, so states compare and print by identity
(`cchi_taurus_state.hpp:9-33`).

### Storage and addressing

- `CacheLine::data[8]` — 8 × uint64 = 64 B (cchi_taurus_component.hpp:200);
  `state` (:201); `PA` is always the line base (:199, ctor :4514-4518).
- The node owns lines in `cacheable`, a
  `std::unordered_map<uint64_t, std::shared_ptr<CacheLine>>` keyed by
  `LineKey()` — the line base with its always-zero low 6 bits shifted out
  (:427-428, :807-811). `IsValid`/`GetState`/`GetCacheLine` are map queries
  (:813-837); a missing key reads as `Invalid`.

### Accessors

- `GetData()` is the raw unchecked view (:4238-4241); the checked
  `Load()`/`Load64/32/16/8` fail (return `std::nullopt`) while the line is
  Invalid or mid-fill (:4243-4364), and `Store()`/`Store64/32/16/8` fail
  unless the line is UC/UD, promoting it to UD on success (:4366-4512).
- "Mid-fill" is `IsREQFilling()`: an allocating read promoted the state at the
  first CompData beat but later beats are still outstanding, so the data plane
  is not consistent yet (:4603-4616, comment :267-270).
- Every checked accessor fires `OnCacheLinePreLoad/PostLoad` /
  `OnCacheLinePreStore/PostStore` through the owning node's hub — which is why
  the line keeps a raw `owner` back-pointer (:185-198).

### Lifetime

- Lines are created on a `Do*` miss (:1072-1074, :1198-1200, :1327-1329) or on
  a snoop to a missing line (:2898-2902), and inserted by `SetCacheLine`, which
  keys the map by the line's own normalized PA (:839-845).
- `Tick()` reaps lines that are `Invalid` with nothing in flight (no pended
  slot, no held active xaction) at the end of every tick. Candidates are indexed
  in `reapSet`/`reapList` and rechecked before the map erase and optional reap
  callback; the map is the sole long-term owner, so the erase actually frees
  the line.
- Lifetime contract (:185-198): a `shared_ptr<CacheLine>` handle from
  `GetCacheLine()` keeps the object alive past reap and node destruction, but
  must not be *used* after the node is gone — the Load/Store accessors
  dereference `owner` to fire events. `GetData()`/`GetPA()`/`GetState()` and
  the in-flight/hazard predicates never touch `owner` and stay safe.
- Event payloads hold lines by `weak_ptr` (`CacheLineEventBase` :282-301):
  their `GetCacheLine()` locks to null after reap/destruction — always check.
  `GetPA()` is value-cached and stays valid.

## 4. The wire model

Seven C-CHI channels, exposed as whole flit structs by value in
`std::optional`. **Peek is non-destructive**; **Pop** resets the line's
channel slot, erases its aged-deque entry, feeds the flit to
`joint.Next*` and fires `OnAccepted*`/`OnDenied*` (§9). Peek and Pop apply
the same active age/random ordering and reply-readiness rules, so Peek's choice
is Pop's choice (barring intervening mutation or different listener outcomes);
both fire the `*ChannelChosen` events for eligible candidates.

| Channel | Direction | API | Backing slots / source | Pop feeds |
|---|---|---|---|---|
| TXEVT | out | `PeekTXEVT` :2515 / `PopTXEVT` :2552 | `pendingEVTChannelTXEVT` via `agedTXEVT` | `joint.NextEVT` (:2585) |
| TXREQ | out | `PeekTXREQ` :2620 / `PopTXREQ` :2703 | `pendingREQChannelTXREQ` via `agedTXREQ`, merged with `prefetchQueue`/`cmoQueue` fronts | `joint.NextREQ` (:2752, :2803, :2840) |
| TXRSP | out | `PeekTXRSP` :2957 / `PopTXRSP` :3028 | `pendingSNPChannelTXRSP` (SnpResp) + `pendingREQChannelTXRSP` (CompAck) via `agedTXRSP` | `joint.NextUpRSP` (:3079, :3117) |
| TXDAT | out | `PeekTXDAT` :3145 / `PopTXDAT` :3248 | `pendingSNPChannelTXDAT0/1` + `pendingEVTChannelTXDAT0/1` via `agedTXDAT` | `joint.NextUpDAT` (:3301, :3341, :3377, :3413) |
| RXSNP | in | `PushRXSNP` :2874 | hazard-parks to `pendingSNPHazardRXSNP` or activates via `joint.NextSNP` (:2935) | — |
| RXRSP | in | `PushRXRSP` :3439 | routes by opcode: Comp :3445, DBIDResp :3573, CompDBIDResp :3615, CompCMO :3657 | `joint.NextDnRSP` |
| RXDAT | in | `PushRXDAT` :3706 | CompData beat store + first-beat state promotion (:3750-3800) | `joint.NextDnDAT` |

Backpressure is asymmetrical by design:

- `PushRXSNP` returns `false` when a snoop to the same line is already in
  flight (:2887-2891) or when `CountInFlightSNP()` reaches `xactionLimitSNP`
  (:2894-2896). A `false` snoop is never stamped or parked (:2904-2906); the
  caller must retry later.
- `PushRXRSP`/`PushRXDAT` always accept (return `true`); a flit whose TxnID
  matches no active transaction is silently dropped (drop sites :3569, :3611,
  :3653, :3694-3697, :3815-3817).

`CountInFlightEVT/REQ/SNP()` are O(1) maintained counters, not full-map
scans. Each counter represents **whole cache lines** in that state, rather
than transactions or beats. Line-scoped `InFlightUpdate` and class-scoped
`InFlightClassUpdate` guards reconcile the existing predicates before and after
each mutation: pending-to-active moves leave the count unchanged, two DAT beats
count once, complete-but-held EVT/REQ transactions remain counted until release,
and a completed SNP leaves the count when its final response completes. A Tick
class with a zero counter returns immediately. The independent
`smoke_inflight_counters.cpp` test compares these counters with reference
scans after every directed transition.

The same transition guards also maintain active-line indexes. `workset*` sets
hold current in-flight keys and `worklist*` vectors provide bounded Tick
iteration; vectors may contain stale keys until compaction, so membership is
always rechecked before use. `reapSet`/`reapList` track only Invalid lines with
no EVT/REQ/SNP activity. This removes the old full-`cacheable` Tick and reap
scans while preserving whole-line lifecycle semantics. `SetCacheLineReapCallback`
installs a notification fired immediately before a record leaves `cacheable`;
the callback must not call back into the node. `GetInFlightEVTCount()` exposes
the maintained EVT count to the gem5 retry-admission logic.

In Cohestra the usage is two-phase (`Instance::Tick`,
../../cohestra/cohestra_instance.cpp:224-295): for each upstream the harness
`PeekTX*`s, pushes the flit into the downstream channel queue, and `PopTX*`s
only when the push succeeded (:251-273); symmetrically it pushes downstream
heads into `PushRXSNP/RXRSP/RXDAT` and pops the downstream queue only on
acceptance — a `false` from `PushRXSNP` leaves the snoop at the queue head to
be retried next tick (:275-291).

## 5. Transaction bookkeeping

### The joint and the xaction classes

`joint` (`Xact::Joint<config>`, :423) is the protocol-FSM container from the
`cchi/xact` layer: every wire flit the node sends or receives is fed to it at
the Pop/Push site, and it owns one `Xact::Xaction<config>` subclass instance
per transaction, constructed through per-opcode decoder tables
(../../xact/cchi_joint.hpp:645-702). The mapping relevant to Taurus:

| `Do*` call | Wire opcode | Xact class | Latched on the line as | Completion signal |
|---|---|---|---|---|
| `DoLoad` :1037 | ReadShared | `XactionCacheableAllocatingRead` | `activeREQ` | `GrantedEvent` future at Comp or all CompData (:2468-2476) |
| `DoStore` :1168 | ReadUnique | `XactionCacheableAllocatingRead` | `activeREQ` | same (:2468-2476) |
| `DoStoreLine` :1297 | MakeUnique | `XactionCacheableDataless` | `activeREQ` | `GrantedEvent` future at Comp (:2431-2439) |
| `DoEvict` :1423 (S/UC) | Evict | `XactionEvict` | `activeEVT` | `EvictedEvent` future at complete (:1903-1908) |
| `DoEvict` :1423 (UD) | WriteBackFull | `XactionWriteBack` | `activeEVT` | `EvictedEvent` future at complete (:2075-2080) |
| `DoPrefetchLoad/Store` :1553/:1602 | StashShared/StashUnique | `XactionStash` | — (lives in `prefetchQueue`) | `PrefetchEmittedEvent` at Pop emission (:2800) |
| `DoCBOClean/Flush/Inval` :1683/:1732/:1781 | CleanShared/CleanInvalid/MakeInvalid | `XactionCMO` | — (`cmoQueue` → `cmoTracker`) | `CMOCompleteEvent` at CompCMO (:3677) |
| `PushRXSNP` :2874 | SnpMakeInvalid/SnpToInvalid/SnpToShared/SnpToClean | `XactionSnoop` | `activeSNP` | response popped from TXRSP/TXDAT |

State effects of completions: ReadShared's first CompData beat promotes to
S/UC/UD by its `Resp` (:3766-3783) and stores each beat's payload
(:3750-3758); ReadUnique promotes UC on Comp or first CompData (:3513-3516,
:3785-3788); MakeUnique promotes **UD** on Comp — a line acquired from Invalid
has no current copy, so its residual content is treated as dirty until the
full-line overwrite lands (:3490-3497). Evict/WriteBack demote to Invalid at
channel-pend time, not at completion (§7, :1513-1522, :1883-1892). Snoop
demotions are computed in `TickSNP` per opcode and current state (:2183-2389).

### Per-line slot families

Each `CacheLine` carries three independent slot families (:203-222):

| Family | Active latch + future | Hazard-pending slots | Channel-pending slots |
|---|---|---|---|
| EVT | `activeEVT` :203, `activeEVTFuture` :204 | `pendingEVTHazardTXEVT` :205, `pendingEVTHazardTXDAT0/1` :207-208 | `pendingEVTChannelTXEVT` :206, `pendingEVTChannelTXDAT0/1` :209-210 |
| SNP | `activeSNP` :212 | `pendingSNPHazardRXSNP` :213 | `pendingSNPChannelTXRSP` :214, `pendingSNPChannelTXDAT0/1` :215-216 |
| REQ | `activeREQ` :218, `activeREQFuture` :219 | `pendingREQHazardTXREQ` :220 | `pendingREQChannelTXREQ` :221, `pendingREQChannelTXRSP` :222 (CompAck) |

A **hazard-pending** slot holds a flit whose action was accepted but must wait
for a same-line hazard to clear; `TickEVT`/`TickSNP`/`TickREQ` re-run the
hazard predicate each tick and transfer the flit to the matching
channel-pending slot when clear (§8). A **channel-pending** slot holds a flit
eligible for selection by the corresponding Peek/Pop, although a reply's ready
time can temporarily hide it (§7). TXRSP/TXDAT slots have matching
`ready*Channel*` fields stamped at pend time (:256-261, :1083-1129); these are
eligibility metadata, not another hazard-pending family. The hazard predicates
themselves (`HasREQHazard` :4618-4680, `HasSNPHazard` :4682-4707, `HasEVTHazard`
:4734-4758, `HasEVTDataHazard` :4760-4777) are detailed in
[HAZARDS.md](HAZARDS.md).

### In-flight predicates

`IsEVTInFlight` (:4532-4560), `IsSNPInFlight` (:4562-4581) and
`IsREQInFlight` (:4583-4601) all include the pended slots — a line with
anything parked or pended counts as busy. Two deliberate asymmetries:

- REQ and EVT hold the line while an `activeREQ`/`activeEVT` pointer exists at
  all, even complete-but-unreleased. Rationale comment (:4595-4599): a
  follow-up request accepted in that window would overwrite
  `activeREQFuture`, and the completing transaction's grant would then fire
  (steal) the new future instead of its own; the EVT case mirrors it
  (:4556-4558).
- SNP instead returns `!activeSNP->IsComplete(glbl)` (:4577-4580): a completed
  snoop no longer blocks.

`CountInFlightEVT/SNP/REQ` (:741-775) fold these over the whole map for the
`xactionLimitEVT/SNP/REQ` admission checks (constructor defaults 16/16/16,
:467-469).

## 6. TxnID management

Upstream-initiated (REQ/EVT) transactions draw TxnIDs from `usedTxnID`, a
`std::vector<bool>` bitmap (:441-444). Its size is `min(xactionLimitTotal, 1
<< txnIdWidth)`: the default total limit is the full field capacity
(constructor default `size_t{1} << config::txnIdWidth`, :470 — 128 entries at
the production 7-bit width), and a larger configured limit is clamped in the
constructor because wider IDs would truncate in `flit.TxnID` and alias on the
wire (:672-683). `AllocateTxnID()` takes the first free bit (:3826-3840);
`FreeTxnID()` is an idempotent bit clear (:3843-3846).

| Action class | Allocated | Freed |
|---|---|---|
| REQ (`DoLoad`/`DoStore`/`DoStoreLine`) | at `Do*` entry, before flit build (:1081, :1207, :1336) | `TickREQ` on joint completion (:2503-2509); on listener rejection of the pend (:1132, :1259, :1282, ...); on joint denial at Pop (:2766); completed-predecessor sweep at the next Pop (:2769-2774) |
| EVT (`DoEvict`) | at `DoEvict` entry (:1451) | `TickEVT` on joint completion (:2083-2090); on joint denial at Pop (:2599); completed-predecessor sweep (:2602-2607) |
| Prefetch (`DoPrefetchLoad/Store`) | at `DoPrefetch*` entry (:1562, :1611) | at Pop emission (:2818-2820) — ExpCompStash=0, so the joint retires the Stash immediately and no completing flit ever arrives (../../xact/cchi_joint.hpp:990-998) |
| CMO (`DoCBO*`) | at `DoCBO*` entry (:1692, :1741, :1790) | on CompCMO in `PushRXRSP` (:3677-3680); on joint denial at Pop (:2861-2864) |
| Snoop | never — the SnpResp/SnpRespData echo the downstream's TxnID (:2332, :2372) | — |

Two subtleties:

- The completed-predecessor sweeps in `PopTXEVT`/`PopTXREQ` (:2602-2607,
  :2769-2774) exist because joint completion needs no Tick — the joint
  completes an Evict on the last copyback-beat pop and a REQ on the CompAck
  pop — while the `TickEVT`/`TickREQ` free sites can never see the
  predecessor once `activeEVT`/`activeREQ` is overwritten.
- Only the request phase carries bitmap TxnIDs. Response-phase flits carry
  downstream-allocated identifiers: CopyBackWrData beats use the WriteBack's
  DBID (:1981, :2029), CompAck uses the granting flit's DBID (:2446, :2485).

## 7. Emission ordering

By default, per-channel pending flits are offered in **action-start order** —
the emission order extracted from the action ages is identical to the `Do*`
call order (snoops ordered by acceptance), across lines and across action
classes. This remains the exact behavior with the default reply randomization
configuration. The mechanism:

- `emissionAgeCounter` (:559-561) is a monotonic source; `NextEmissionAge()`
  (:847-851) stamps **once per action**, at the `Do*` entry points and at
  snoop acceptance (:2904-2906), hazard pend included.
- One age per action covers every flit the action emits (contract comment
  :224-231): `ageTXEVT` covers the TXEVT flit and its CopyBackWrData beats,
  `ageTXREQ` the TXREQ flit and its CompAck, `ageSNP` the SnpResp and the
  SnpRespData beats (:232-234). An age is only consulted while a slot it
  covers is pending, and the in-flight predicates include the pended slots, so
  a covered age field is never overwritten early.
- Four deques of `(age, LineKey)` pairs mirror the channel slots: `agedTXEVT`,
  `agedTXREQ` (per-line REQs only), `agedTXRSP` (SNP-RSP and CompAck slots),
  `agedTXDAT` (SNP-DAT0/1 and EVT-DAT0/1 slots) (:563-573).
- `AgedPush` (:853-862) inserts at the first position with a greater age —
  sorted insertion, not plain `push_back`, because hazard-to-channel
  transfers, Tick-time pends and hash-order Tick releases can push ages older
  than entries already in the deque (rationale comment :578-582); the common
  case (new entry is youngest) stays O(1) at the back. `AgedErase` (:864-873)
  removes the first matching pair.
- Every channel-slot assignment goes through the `Pend*Channel*` helpers
  (:875-931), so a pended slot and its deque entry can never diverge. Peek/Pop
  self-heal the remaining staleness — an entry whose line is gone, whose slot
  is empty, or whose age no longer matches is erased on encounter
  (:2525-2534, :2643-2652, :2980-2989, :3168-3179).

The full contract comment (:660-674), summarized:

1. Default per-channel order = action-start order; no action class
   (DoLoad/DoStore*/DoEvict, DoPrefetch*, DoCBO*) has fixed precedence.
2. Hazard-parked flits retain their action-start age when released to the
   channel. That age determines their default-order position and is the stable
   input to optional reply ranking and delay sampling.
3. Per-line slot priorities always take precedence over cross-line age or rank
   selection *within* a line: TXRSP emits SnpResp before CompAck (Peek
   :3227-3267, Pop :3319-3405); TXDAT emits SNP-DAT0 → SNP-DAT1 → EVT-DAT0 →
   EVT-DAT1 (Peek :3458-3536, Pop :3589-3735). Normal channel-pending beats
   therefore emit DAT0 before DAT1; the existing listener-forced data-hazard
   path is the documented exception that can make an unforced DAT1 precede a
   parked DAT0. A line selected through an older slot's deque entry may emit
   its newer slot's flit — and Pop erases the popped slot's own entry, not
   necessarily the entry that won the iteration (:3355-3358, :3399-3402,
   :3624-3629).
4. A `PreChannelChosen` cancellation keeps the flit pended, skips that line
   for the rest of the call and picks the next candidate in the active
   age/random ordering. A cancelled candidate is never re-offered within the
   same call and is offered normally again on the next call, subject to its
   rank and readiness. Single-slot channels (TXEVT, and per-line entries in
   TXREQ) need only a `continue`; multi-slot channels (TXRSP, TXDAT) keep a
   per-call skip set of line keys (:3187-3208, :3417-3437).

**TXRSP/TXDAT reply randomization.** `ReplyRandomizationConfig` defaults to
`randomPendingPick=false`, `delayMin=delayMax=0`, and `seed=0` (:84-89), which
preserves the exact legacy age-order and immediate-reply behavior. With
`randomPendingPick=true`, only outbound replies change order: CompAck/SnpResp
on TXRSP and CopyBackWrData/SnpRespData on TXDAT. TXREQ and TXEVT never use
random ranking, and Cohestra's cross-channel polling order is unchanged.

Random mode computes a stable per-action rank with an explicit 64-bit mixer of
the node seed and action age (:914-933), then sorts candidates by rank with
action age and line key as tie-breakers (:1015-1019, :1058-1062). There is no
mutable RNG and no random draw in Peek/Pop, so repeated polling and
cancellation do not consume entropy or change existing ranks. The same seed
and action ages reproduce the same ranks and ordering of any given pending set.
Both DAT beats of a reply use their action's rank; normal channel-pending
beats retain per-line DAT0-before-DAT1 priority.

**Reply eligibility delays.** Delay sampling is independent of random ordering.
When a reply is constructed at time `T`, it receives one delay `D` sampled
deterministically from its seed/age in inclusive `[delayMin,delayMax]`, and
becomes eligible at saturating `T+D` (:935-959, :1083-1129). CopyBackWrData
beats constructed together share one ready time even if a listener-forced
hazard parks and releases them in different ticks. `delayMax=0` means
immediate eligibility; `delayMin=delayMax` gives a fixed delay. The setter
rejects `delayMin>delayMax` without changing the old config (:846-860). These
are raw `Tick(time)` units, not necessarily clock cycles. Both beats of a
two-beat reply share the action's rank and ready time.

An unready highest-priority reply skips its whole line for that Peek/Pop call,
fires no `PreChannelChosen` event, and lets the next candidate in the active
ordering proceed. Delaying a reply does not delay TXREQ/TXEVT requests, postpone
grant futures, or change hazard predicates or protocol state at pend time;
normal Pop-driven completion still occurs when the reply is eventually emitted.
Configure the feature at construction/quiescent time so existing replies do not
change rank or ready time mid-flight.

**Current checker caveat.** A dirty SnpRespData response fixes its Resp and data
when TickSNP determines the transition, while the observer-only
`CacheStateMap` currently evaluates that response at the later Pop. If a legal
same-line request overtakes the delayed response and narrows the shadow state
set first, the observer can falsely report `XACT_DENIED_STATE_RESP_SNPRESPDATA`
for a legal late response. This timing/history limitation predates reply
randomization and does not indicate a CacheLineDataMonitor data mismatch; it
should be fixed by recording the response-determination state separately from
the later wire-emission check.

**The TXREQ merge**: TXREQ has no deque entries for prefetches and CMOs and
always retains age ordering. Peek/Pop three-way-merge per-line REQs from
`agedTXREQ` with the `prefetchQueue` and `cmoQueue` fronts — FIFO push order is
call order, so each front is its class's oldest — as an O(1) minimum
re-evaluated at every iteration, so a cancelled per-line REQ yields to an older
prefetch/CMO next (:2622-2626, :2705-2709). A queue candidate older than every
surviving per-line candidate dispatches after the walk (:2669-2699, :2783-2870).

## 8. The Tick pipeline

`Tick(time)` (:1829-1852) stamps `this->time`, runs three phases in a fixed
order, then reaps idle Invalid lines (:1838-1851, §3):

```
TickEVT (:1854-2093)   per line, hash order:
  1. pendingEVTHazardTXEVT: re-run HasEVTHazard; when clear, transfer to the
     channel slot with its original action age (:1861-1878) and demote the
     line to Invalid *at release* (:1883-1892)
  2. activeEVT: after DBIDResp, build the two CopyBackWrData beats and
     hazard- or channel-pend them (:1915-2073); fire EvictedEvent once when
     the xaction completes (:1903-1908, :2075-2080); free its TxnID and reset
     the latch (:2083-2090)
TickSNP (:2095-2393)   per line:
  1. pendingSNPHazardRXSNP: re-run HasSNPHazard; when clear, feed the snoop to
     the joint and latch activeSNP (:2102-2138)
  2. activeSNP not yet answered: compute the state transition and
     Resp/RetToSrc from opcode × current state (:2183-2319), pend SnpResp or
     two SnpRespData beats, commit the new state (:2321-2389) — gated by the
     same-Tick EVT rule below
TickREQ (:2395-2512)   per line:
  1. pendingREQHazardTXREQ: re-run HasREQHazard; when clear, transfer to the
     channel slot with its original age (:2402-2422)
  2. activeREQ: on grant condition, fire OnCacheLineGranted + the
     activeREQFuture (:2431-2439, :2468-2476); pend CompAck after the first
     granting flit (:2441-2461, :2478-2500); free TxnID on completion
     (:2503-2509)
```

The **EVT → SNP → REQ order is load-bearing** for the hazard rules
([HAZARDS.md](HAZARDS.md)): EVT is always the highest-priority transaction
(comment :1891), so evictions leave their queues before snoops are processed
and before REQs are re-detected. The sharp edge is the **same-Tick EVT gate**
in TickSNP (:2179-2181, with a worked example in the comment :2155-2178):
when a snoop and an EVT left their queues in the same tick, the snoop's state
change and response wait for the EVT's first response (Comp for Evict, Comp or
CompDBIDResp for WriteBack — `GotEVTComp` :4709-4732). This cannot deadlock:
the gate only bites in that same-Tick window, and the EVT never waits for the
SNP (`HasEVTHazard` does not consult SNP state, :4752-4755).

Iteration within each phase follows the active worklist's transition/insertion
order rather than `unordered_map` hash order; cross-line fairness is not the
Tick's job — default wire order is re-established by the aged deques at Peek/Pop
time, while TXRSP/TXDAT can instead use their stable seeded ranks (§7). This is
exactly why `AgedPush` must tolerate out-of-order inserts in both modes.

## 9. The event system

`events` is a `std::shared_ptr<EventHub>` (:176) holding 77
`Gravity::EventBus<TEvent>` members (:83-174), one per concrete event class in
`cchi_taurus_component_events.hpp`. Buses fire synchronously, in listener
priority order (`Register` inserts by priority,
../../../../common/eventbus.hpp:588-592 and :551-560; `Fire` walks the vector,
:691-698). Listeners are built with `MakeListener(name, priority, callable)`
(../../../../common/eventbus.hpp:253-271) and removed by name
(`Unregister`/`UnregisterAll`, :597-642; hub-wide `EventHub::Clear`,
cchi_taurus_component.hpp:3976-4061). The event object is passed as a mutable
reference — listeners may both observe and, for the mutator-capable classes
below, alter the outcome.

Hub census by group (bus members :85-169):

| Group | Count | Members |
|---|---|---|
| EVT pipeline | 22 | OnEVT{Pre/Post}{HazardDetection, HazardPending, ChannelPending, HazardToChannelPending} (8), OnEVTCacheState{Pre/Post}Demotion (2), OnEVTData{Pre/Post}{HazardDetection, HazardPending, ChannelPending, HazardToChannelPending} (8), OnEVT{Pre/Post}ChannelChosen + OnEVTUpDAT{Pre/Post}ChannelChosen (4) |
| SNP pipeline | 14 | OnSNP{Pre/Post}{HazardDetection, HazardPending} (4), OnSNPCacheState{Pre/Post}Demotion (2), OnSNPResp{Pre/Post}ChannelPending (2), OnSNPRespData{Pre/Post}ChannelPending (2), OnSNPUpRSP/UpDAT{Pre/Post}ChannelChosen (4) |
| REQ pipeline | 14 | OnREQ{Pre/Post}{HazardDetection, HazardPending, ChannelPending, HazardToChannelPending} (8), OnREQCompAck{Pre/Post}ChannelPending (2), OnREQ{Pre/Post}ChannelChosen + OnREQUpRSP{Pre/Post}ChannelChosen (4) |
| Acceptance | 10 | OnAccepted{EVT, SNP, REQ, DnRSP, UpRSP, DnDAT, UpDAT, Prefetch, CMO, CompCMO} |
| Denial | 10 | the OnDenied* twins of the above |
| CacheStateMap | 2 | OnCacheStateMapDenied{Request, Response} |
| Cache line | 5 | OnCacheLineGranted, OnCacheLine{Pre/Post}{Load, Store} |

Event payloads derive from small bases: the node reference
(`UpstreamNodeEventBase`, cchi_taurus_component_events.hpp:147-158), the line
(:160-174), a **mutable flit reference** (:21-110 — listeners can rewrite the
flit about to be pended/emitted), and the xaction handle (:228-244).

Mutator-capable classes (all enforced by the component's call sites):

- `*PreHazardDetectionEvent::SetHazard()` — forces the hazard path (hazard
  flag held by reference; e.g. :686-701, :843-858, :981-996, :1163-1178).
- `*PreHazardPendingEvent` (EVT/SNP/REQ) and `*PreChannelPendingEvent`
  (EVT/REQ) `SetDenial()`/`Deny()` — rejects the action; the component frees
  the TxnID and returns a rejected future (or `false` from `PushRXSNP`)
  (:720-776, :1016-1035, :1198-1253).
- `*PreChannelChosenEvent` derives `Gravity::CancellableEvent` — cancellation
  skips the candidate per §7 rule 4 (:938-946, :959-967, :1120-1128,
  :1141-1149, :1314-1322, :1335-1343).
- `*CacheStatePreDemotionEvent::SetNextState()` — demotion-only: the new state
  must be the computed next state or strictly lower (:113-131).

Callback contract (:29-34): bound callbacks and event listeners run
synchronously, mid-iteration over the node's containers (Tick* loops, channel
Peek/Pop paths, response dispatch). A callback must **not** call back into the
same node (no `Do*`, Peek/Pop, Push, or Tick) — doing so can invalidate the
container iteration state the caller is still using. Note that Peek fires the
same `*ChannelChosen` events as Pop, so observers see each chosen flit twice
across a Peek→Pop pair.

Listener recipes (hazard injection, flit mutation, snoop forcing) live in
[USER_GUIDE.md](USER_GUIDE.md); the semantics of the hazard paths these events
gate are in [HAZARDS.md](HAZARDS.md).

## 10. The CacheStateMap shadow checker

`cacheStateMap` (`Xact::CacheStateMap<config>`, :425) is an observer-only
shadow of the line states, enabled by `enableCacheStateMap` (default true,
:457-461, :475). Every wire flit accepted by the joint is also fed to it
through the `CheckCacheStateTX*/RX*` helpers (:933-1034, declaration comment
:602-604); a non-ACCEPTED result fires `OnCacheStateMapDeniedRequest` or
`OnCacheStateMapDeniedResponse` and **never alters control flow**. The flag is
meant for construction-time or quiescent toggling — an off→on flip mid-run
leaves the shadow stale for lines already touched (:457-460).

The checker is reachable via `GetCacheStateMap()` (:513, impl :789-799) for
inspection and control: `Set`/`Get` per address, `Excavate` (raw tracked
state) vs `Evaluate` (state widened by the silent-transition assumptions),
and the four silent-transition toggles `SetSilentEviction/Sharing/Store/
Invalidation` (../../xact/cchi_xact_state.hpp:309-324).

## 11. The `config` template parameter and the 256-bit data plane

`config` is a `FlitConfigurationConcept`
(../../spec/cchi_protocol_flits.hpp:96-97) — the `FlitConfiguration` parameter
pack (:44-53): `ComponentType`, `TxnIDWidth`, `DBIDWidth`,
`UpstreamNodeIDWidth`, `DownstreamNodeIDWidth`, `WayIndexWidth`, `DataWidth`,
`TagAliasWidth`, `UWPersistEnable`, `UWPredictEnable`. All flit field types
(`txnid_t`, `addr_t`, the `hasTagAlias` guard, ...) derive from it, so the
whole component re-specializes by changing one alias.

The production configuration is `Cohestra::CommonFlitConfigurationType1 =
FlitConfiguration<TYPE_1, 7, 8, 5, 5, 4, 256, COHESTRA_TAGALIAS_WIDTH, true,
true>` (../../cohestra/cohestra_interface.hpp:19-20; the TagAlias width is a
build-time knob defaulting to 0, :15-17, which is why every TagAlias access is
`if constexpr (Flits::REQ<config>::hasTagAlias)`-guarded, e.g.
cchi_taurus_component.hpp:1097-1098). Consequences baked into the component:

- **7-bit TxnID** → the default total in-flight limit is 128 (§6).
- **256-bit data beats** → a 64 B line is exactly 2 DAT beats. The two-beat
  data plane is hardcoded throughout: `DataID` 0/1 carrying `data[0..3]` /
  `data[4..7]` in both directions (build :1990-1994, :2038-2042, :2341-2359;
  store :3750-3758 with the `dataID < 2` guard), full-line `BE = 0xFFFFFFFF`,
  the SNP-DAT0→SNP-DAT1 and EVT-DAT0→EVT-DAT1 slot pairs (§5). Configs with
  `DataWidth != 256` are not supported by this component even though the spec
  layer admits them.

Smoke tests instantiate a deliberately smaller configuration (4-bit TxnID,
2-bit way index, still 256-bit data;
../../test/smoke_upstream_node.cpp:13-22).

## Where to go next

- [../README.md](../README.md) — component overview, build, quick start.
- [TRANSACTIONS.md](TRANSACTIONS.md) — per-transaction message sequences and
  state walks for every `Do*` call and snoop opcode.
- [HAZARDS.md](HAZARDS.md) — the same-line hazard rules, their predicates, and
  the listener injection points.
- [USER_GUIDE.md](USER_GUIDE.md) — API usage: futures, event listeners,
  checker control, denial taxonomy.
- [DOCUMENTATION_GUIDE.md](DOCUMENTATION_GUIDE.md) — conventions for this
  documentation set.
