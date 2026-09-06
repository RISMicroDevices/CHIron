# Venus — Transaction Dataflows

Detailed per-opcode dataflow through the Venus home. This is the executable design spec
for `venus_mshr.sv`; every phase name below (`PH_*`) appears verbatim in the RTL, and the
RTL comments cross-reference the section numbers here.

Conventions:

- **Slot** = one `venus_mshr` tracker (PARALLELISM slots). A REQ transaction
  occupies its line exclusively (same-address CAM, PROTOCOL §7.3); EVT
  transactions may share a line with an in-flight REQ slot (§7.3, deadlock
  avoidance), and a read-flow slot stalls its AXI read behind any colliding
  not-yet-landed write (M5 write-hold, §7.3).
- **Line buffer** = per-slot 512-bit `s_data` + 64-bit byte strobe `s_strb`.
  "Merge PD" = SnpRespData beats are written into the buffer under their BE.
  "Merge write" = NonCopyBack/CopyBack WrData beats are written under their BE (write
  wins over previously merged PD on overlap).
- **Backends** are shared, single-outstanding, arbitrated per cycle among slots:
  directory (DIR), snoop filter (SF), AXI master(s). Emission: ≤1 SNP, ≤1 DnRSP,
  ≤1 DnDAT per cycle globally.
- **DBID** = per-port home-allocated ID (PROTOCOL §6); allocated on the first
  DBID-bearing downstream flit, freed per PROTOCOL §5.2.
- Directory snapshot fields: `hit, state∈{S,U}, sharers, owner, aliases,
  full, victim{way,live,line,state,sharers,owner}` — `aliases` is the
  per-holder TagAlias record (PROTOCOL §7.7; `aliases[p]` meaningful only
  while holder bit `p` is set). SF snapshot: `presence, maybe_all`.

Phase list: `PH_IDLE, PH_DIR_LU, PH_SF_LU, PH_SNOOP, PH_VIC_SNP, PH_VIC_WB, PH_VIC_RM,
PH_VIC_RSF, PH_AXI_RD, PH_AXI_WR, PH_DBID, PH_RESP, PH_COMMIT, PH_SF_UPD, PH_WAIT_ACK,
PH_WAIT_WR, PH_WAIT_POP, PH_DONE`.

---

## 1. ReadNoSnp (0x02) — §T1

Non-coherent read. No directory, no snoop filter, no snoops, no DBID, no CompAck.

```
upstream                Venus (home)                 AXI
   |  REQ ReadNoSnp(Size)   |                          |
   |----------------------->|  PH_AXI_RD: AR(len per Size, addr = aligned base)
   |                        |------------------------->|
   |                        |  R beats -> s_data       |
   |  CompData x N (Resp=UC, DataID from Addr[5], DBID=0)
   |<-----------------------|  PH_RESP -> PH_DONE      |
```

- Phases: `PH_AXI_RD → PH_RESP → PH_DONE`.
- Beats: N = 1 for Size ≤ 32 B (DataID = Addr[5]), else 2 (DataID 0,1). Resp constant UC.
- Retire: last CompData beat accepted into the port's DnDAT FIFO.

## 2. ReadOnce (0x03) — §T2

Coherent peek, no allocation. The requester stays I; directory is not modified.

- Phases: `PH_DIR_LU → [PH_SNOOP] → PH_AXI_RD | PH_AXI_WR → PH_RESP → PH_DONE`.
- DIR LOOKUP outcome:
  - hit U, owner ≠ requester: `PH_SNOOP` — SnpToClean to the owner only.
    - SnpRespData_UC_PD: merge PD (full line) → `PH_AXI_WR` writes the merged line back
      (memory becomes clean; owner's UC is now genuinely clean) → CompData from buffer.
    - SnpResp_UC/SC/I: `PH_AXI_RD` (memory is current).
  - hit S or miss: `PH_AXI_RD` (no snoop; miss needs no victim — nothing is allocated).
- `PH_RESP`: CompData×N (N per Size, constant Resp UC), then `PH_DONE` (no DBID/CompAck).

## 3. ReadShared (0x04) — §T3

Allocating shared read, with promotion (PROTOCOL §7.4).

```
REQ -> DIR LU -> [victim] -> [SF LU] -> [snoop] -> AXI rd | (PD merge -> AXI wr)
    -> dir GRANT -> SF UPDATE -> CompData x2 (DBID) -> CompAck -> retire
```

- Phases: `PH_DIR_LU → [PH_VIC_*] → [PH_SF_LU] → [PH_SNOOP] → PH_AXI_RD|PH_AXI_WR →
  PH_COMMIT → PH_SF_UPD → PH_RESP → PH_WAIT_ACK → PH_DONE`.
  Allocating kinds **commit the directory before responding**: a GRANT-full
  retry (§T15) then re-plans with `s_rbeats` still empty, so no response flit
  can ever be emitted twice.
- Snoop plan (mask excludes requester):
  - miss: none. `PH_AXI_RD`.
  - hit S: none (memory is current). `PH_AXI_RD`.
  - hit U, owner ≠ requester: SnpToShared → owner. PD → merge + `PH_AXI_WR` (writeback);
    else `PH_AXI_RD`.
  - hit U, owner == requester: protocol-odd (requester re-reads its own unique line);
    no snoop, `PH_AXI_RD`, documented as serving memory's copy.
- Completion Resp + directory image:
  - other holders absent after snooping (miss path): **promote** — CompData UC,
    GRANT `{U, owner=req}`.
  - else: CompData SC, GRANT `{S, sharers = (post-snoop holders) | req}` —
    U-hit: `{owner, req}`; S-hit: `sharers | req`.
- Alias mismatch (`TAGALIAS_W>0`; the requester is a recorded holder with a
  *different* recorded alias, PROTOCOL §7.7): the requester is added to the
  snoop mask with a SnpToInvalid override for that target, folded into the
  same `PH_SNOOP`. A dirty stale-alias copy answers SnpRespData_I_PD — its
  data both merges into the response (CompData serves those bytes) and writes
  back to memory (`PH_AXI_WR`). The commit records the requester's new alias.
  A matching alias takes the unmodified flow above with the requester excluded
  from the mask — no self-snoop, guaranteed. Sibling mechanism: victim
  back-invalidation (§T15).
- DBID allocated on the first CompData beat, echoed on both beats; freed by CompAck.
- Retire: CompAck seen AND dir GRANT AND SF UPDATE done (CompAck may arrive right after
  beat 0 — it is latched, not acted on, until the commit phases complete).

## 4. ReadUnique (0x10) — §T4

Allocating unique read (optionally dataless upgrade).

- Phases as §T3 (commit before respond). Snoop plan (mask excludes requester):
  - miss: none, `PH_AXI_RD`.
  - hit S: SnpToInvalid → `sharers ∖ {req}`. Memory is current: `PH_AXI_RD`.
  - hit U, owner ≠ requester: SnpToInvalid → owner. PD → merge + `PH_AXI_WR` writeback;
    else `PH_AXI_RD`.
  - hit U, owner == requester: no snoop, no AXI (upgrade).
- Completion (`req_hit` = the requester is a recorded current holder in the
  directory snapshot; a recorded holder's copy is current by construction):
  - CompData×2 whenever `ExpCompData=1` **or** `!req_hit`, Resp **UC always**
    (never `*_PD` — Taurus ignores PD here and goes UC; Venus cleans PD to
    memory instead). DBID on beat 0. ExpCompData is only a hint: a requester
    whose copy is not current (not recorded — e.g. it was snooped away before
    its queued RU was emitted) must always receive CompData.
  - Comp (DnRSP, carries DBID) only when `ExpCompData=0` **and** `req_hit`
    (a genuine from-Shared upgrade with a current copy).
- Alias mismatch (PROTOCOL §7.7): as §T3 — the requester joins the snoop mask
  with a SnpToInvalid override for that target, folded into `PH_SNOOP`; a
  dirty old-alias copy's SnpRespDataPD merges into the CompData **and** writes
  back to memory; the commit records the new alias. Since the requester's copy
  is snooped away, the completion is always CompData, never dataless Comp,
  even when ExpCompData=0 (`resp_is_dat`/`need_axi_rd` gain `s_alias_inv`; a
  clean stale copy forces an AXI refill despite the hit). The same-alias case —
  including the common S→U store-after-load upgrade — takes the unmodified
  flow with the requester excluded from the mask (no self-snoop, guaranteed).
  Sibling mechanism: victim back-invalidation (§T15).
- GRANT `{U, owner=req}`; SF UPDATE `{req}`. Retire on CompAck + commits done.

## 5. MakeUnique (0x12) — §T5

Dataless full-line store intent. No AXI traffic.

- Phases: `PH_DIR_LU → PH_SF_LU → PH_SNOOP → PH_COMMIT → PH_SF_UPD →
  PH_RESP → PH_WAIT_ACK → PH_DONE`.
- Snoop: SnpMakeInvalid → all present ∖ {req}. Dirty data at peers is *dropped* by
  SnpMakeInvalid — correct because the requester will overwrite the whole line.
- Alias mismatch (PROTOCOL §7.7): the requester joins the mask with a
  SnpToInvalid override for that target, folded into `PH_SNOOP`; a dirty
  stale-alias copy's SnpRespDataPD is merged and written back to memory via
  the standard PD path (§16.2) — MakeUnique itself carries no response data.
  The commit records the new alias. A matching alias takes the unmodified flow
  with the requester excluded from the mask (no self-snoop, guaranteed).
  Sibling mechanism: victim back-invalidation (§T15).
- On a miss the GRANT allocates U{req} (victim flow applies); the commit
  precedes the Comp exactly like the allocating reads (§T3 note).
- `PH_RESP`: Comp (DBID). GRANT `{U, owner=req}`. SF UPDATE `{req}`.
- Retire on CompAck + commits done.

## 6. WriteNoSnpPtl (0x08) / WriteNoSnpFull (0x09) — §T6

Non-coherent write. No directory, no snoops. Comp is sent only after the data has
landed in memory (split form; DBIDResp first).

```
REQ -> DBIDResp(DBID) -> NCBWrData x N -> AXI write -> Comp -> retire
```

- Phases: `PH_DBID → PH_RESP(DBIDResp) → PH_WAIT_WR → PH_AXI_WR → PH_RESP(Comp) →
  PH_DONE`.
- `PH_WAIT_WR`: collect NonCopyBackWrData beats (N per Size; distinct DataIDs) merged
  under BE into the buffer. DBID freed at the last beat.
- `PH_AXI_WR`: address = line base (Full) or Size/Addr[5]-aligned base (Ptl); wstrb =
  merged strobes (Full: all-ones); AxLEN = N−1.
- `PH_RESP(Comp)` after AXI B; slot holds the line busy until then (same-address CAM),
  so a later read to the line always observes the write.

## 7. WriteUniquePtl (0x0A) / WriteUniqueFull (0x0B) — §T7

Coherent write without data fetch; all copies (incl. the requester's) invalidated;
the requester ends I. Directory entry removed.

- Phases: `PH_DIR_LU → PH_SF_LU → PH_SNOOP → PH_DBID → PH_RESP(DBIDResp) → PH_WAIT_WR →
  PH_AXI_WR → PH_RESP(Comp) → PH_COMMIT(REMOVE) → PH_SF_UPD(∅) → PH_DONE`.
- Snoop (mask **includes** requester, PROTOCOL §7.2):
  - Full: SnpMakeInvalid — data unneeded (full overwrite).
  - Ptl: SnpToInvalid — merge any PD (possibly from the requester itself) into the
    buffer first; the write data then overwrites under its BE (write wins), preserving
    dirty bytes outside the write mask.
- `PH_WAIT_WR` + merge, `PH_AXI_WR` (Full: full strb; Ptl: union of PD and write strobes),
  Comp after AXI B.
- DIR REMOVE (whole entry), SF UPDATE presence = ∅ (entry dropped).

## 8. CleanShared (0x0C) — §T8

CMO: push dirty data of the unique owner to the point of coherency; states unchanged.

- Phases: `PH_DIR_LU → [PH_SNOOP] → [PH_AXI_WR] → PH_RESP(CompCMO) → PH_DONE`.
- Only a U-state owner can hold dirty data (S copies are clean by definition):
  - hit U: SnpToClean → owner (**may be the requester itself**, §7.2). PD → merge →
    `PH_AXI_WR` writeback. No PD: straight to CompCMO.
  - hit S / miss: nothing to clean.
- No directory or SF update. Directory stays U{owner} — now known-clean.

## 9. CleanInvalid (0x0D) — §T9

CMO: invalidate every copy (incl. the requester's), writing back dirty data.

- Phases: `PH_DIR_LU → PH_SF_LU → PH_SNOOP → [PH_AXI_WR] → PH_COMMIT(REMOVE) →
  PH_SF_UPD(∅) → PH_RESP(CompCMO) → PH_DONE`.
- Snoop: SnpToInvalid → all present **incl. requester**. PD merged → AXI writeback.
- DIR REMOVE, SF UPDATE ∅. Retire after CompCMO pushed.

## 10. MakeInvalid (0x0E) — §T10

CMO: invalidate every copy (incl. the requester's), discarding dirty data.

- As §T9 but SnpMakeInvalid (never returns data — dirty drop is the CMO's semantics),
  no AXI writeback. DIR REMOVE, SF UPDATE ∅, CompCMO.

## 11. StashShared (0x00) / StashUnique (0x01) — §T11

Prefetch hint with implicit target. Venus keeps nothing it could stash into for a
Type-1 requester, so this is a no-op (user decision).

- Phases: `PH_RESP(CompStash)` iff ExpCompStash=1 (the REQ union bit), else straight
  `PH_DONE`. No DIR/SF/snoop/AXI, no DBID, no CompAck.
- Taurus always issues Stash* with ExpCompStash=0 and retires them at emission, so
  CompStash is never emitted in the V3 flow (but remains protocol-complete).

## 12. EvictBack (0x1E) / EvictClean (0x1F), Atomics (0x20–0x31) — §T12

Rejected at admission: the flit is popped and dropped, a `$warning` is emitted in
simulation, no slot is allocated. Documented as reserved (PROTOCOL §4.1, §8).

## 13. EVT Evict (0) — §T13

Clean eviction notice. The upstream is already I (demoted at EVT pend time).

- Phases: `PH_COMMIT(REMOVE port) → PH_SF_UPD(REMOVE port) → PH_RESP(Comp) →
  PH_WAIT_POP → PH_DONE`.
- DIR REMOVE in PORT mode: clear the port's sharer bit; drop the entry if it becomes
  empty; U{port} → drop entry. Idempotent (no-op if absent — the line may have been
  back-invalidated already).
- Comp carries DBID=0 (no allocation, no CompAck).
- `PH_WAIT_POP`: the slot retires only when the Comp actually fires on the port's DnRSP
  wire (`txrsp_fire` with matching TxnID) — the upstream may reuse the line / its TxnID
  immediately after seeing Comp.

## 14. EVT WriteBackFull (1) — §T14

Dirty eviction, whole line. The upstream is already I.

```
EVT -> CompDBIDResp(DBID) -> CopyBackWrData x2 -> AXI write -> dir REMOVE -> SF ∅ -> retire
```

- Phases: `PH_DBID → PH_RESP(CompDBIDResp) → PH_WAIT_WR → PH_AXI_WR →
  PH_COMMIT(REMOVE port) → PH_SF_UPD(REMOVE port) → PH_DONE`.
- DBID allocated before the grant; freed at the last CopyBackWrData beat
  (Resp I_PD, BE all-ones, DataID 0/1 expected).
- Comp is inside CompDBIDResp (early by design): M5 (PROTOCOL §5.8) is
  enforced by the read-flow write-hold — any same-line read slot stalls its
  AXI read until this slot's write has landed (§7.3), and later REQs stall at
  admission until retirement.
- DIR REMOVE and SF REMOVE are PORT-mode (evictor's own bit only), so they
  commute freely with a same-line REQ slot's GRANT (§7.3 EVT sharing).

## 15. Victim back-invalidation (sub-flow) — §T15

Entered from `PH_DIR_LU` on a miss with a full set, for allocating reads only
(ReadShared/ReadUnique). ReadOnce never allocates; non-allocating ops never enter.

```
DIR LU: miss, full, victim{way,line,state,holders}
  -> PH_VIC_SNP : SnpToInvalid -> victim holders (skip if victim already dead)
       PD merged into the buffer
       (o_vwb_hold asserted: same-line AXI fills stall from here, §7.3 M5)
  -> PH_VIC_WB  : AXI write victim line (if PD), on the channel the *victim*
       line hashes to (o_axi_sel switches to venus_axi_map(s_vline) for this
       phase — the victim is not the admitted line, so the admission-time
       select c_axi would misroute it under NUM_AXI > 1)
       (o_vwb_hold released when the write lands)
  -> PH_VIC_RM  : DIR REMOVE of the victim line (atomic compare-and-remove)
  -> PH_VIC_RSF : SF UPDATE(victim line, ∅)   (skipped on a compare-fail no-op)
  -> resume the main flow (main-line SF lookup, snoop plan, AXI fill, ...)
```

- Victim selection: invalid-way-first, else tree-PLRU (reported by the DIR LOOKUP).
  The victim's holders may include the requester of the allocating read (self-snoop
  situation, answered normally per §7.2).
- **Victim/main-line mutual hazard protection (bidirectional).** The victim line
  participates in the same-address discipline in both directions. (a) At the arm
  decision (`PH_DIR_LU` response, miss+full): if the proposed victim line is
  another slot's in-flight main line, the slot does NOT arm — it re-issues the
  DIR LOOKUP and re-plans from a fresh proposal once the blocker completes
  (`vic_blk`; bounded retry, same idiom as the allocation race below, but
  *throttled*: `VIC_RETRY_THROTTLE` cycles pass between re-lookups, because the
  fixed-priority directory arbiter would otherwise let an unthrottled retry
  starve a directory-bound blocker slot into a priority-inversion livelock —
  the gap guarantees the blocker gets the directory). A pure
  stall would act on the stale image: its snoops would kill the copy the
  blocker's pending commit just granted while the compare-and-remove no-ops —
  directory desync. (b) Symmetrically, a REQ candidate whose line has an armed
  victim sub-flow stalls at admission (`o_vic_act`, asserted from the arm cycle
  itself): the victim entry still reads as a hit until `PH_VIC_RM`, so an
  admitted flow would otherwise plan on meta that is about to be removed. Both
  directions are deadlock-free: the blocker's main line is directory-resident
  (a missed line can't be proposed as victim), so it hit or already allocated,
  never needs a victim itself, and always completes independently; and a victim
  sub-flow never waits on admissions, so it always clears.
- **Compare-and-remove.** With the mutual protection above, the surviving races
  on the victim entry are victim-vs-victim (two slots proposing the same victim
  line in the same set) and PORT-mode removals by an EVT sharing the line
  (§7.3). Dropping the entry on stale meta would orphan a live owner whose
  snoop was never issued (silent dirty-data loss). `PH_VIC_RM` therefore
  issues the LINE-mode REMOVE with the match image `{state,sharers,owner}` captured
  at LOOKUP; the directory drops the entry only while the meta still matches
  (`cmd_men`). On a mismatch the REMOVE is a no-op (`rsp_hit=0`), the SF clear is
  skipped (the line may have been re-granted; erasing its SF presence could drop the
  new owner's record), and the main flow continues — the set is still full, so the
  commit GRANT answers `full` and the slot re-runs `PH_DIR_LU` with a fresh victim
  (bounded retry, same as the allocation race below). Residual corner: a remove and
  an *identical-image* re-grant of the victim line inside the window is not
  distinguishable by the compare (classic ABA); it requires the same line to be
  evicted and re-allocated to the same state/owner within one victim sub-flow.
- After `PH_VIC_RM`/`PH_VIC_RSF` frees a way, the main flow's GRANT (invalid-way-first)
  lands in it. A racing slot could theoretically claim the freed way first: the
  GRANT then answers `full` and the slot re-runs `PH_DIR_LU` (bounded retry).
  Because allocating kinds (RS/RU/MU) commit the directory **before** emitting
  any response flit, such a retry re-plans with response progress still empty —
  no Comp/CompData can ever be duplicated by a retry.
- **Memory ordering.** A REQ-kind transaction can no longer overlap a victim
  sub-flow on its line (mutual protection above); the remaining concurrent
  actor on the victim line is an EVT sharing it (§7.3), whose PORT-mode
  removal is covered by the compare-and-remove. The data side is the M5
  extension (§7.3): from
  `PH_VIC_SNP` until the victim writeback lands, `o_vwb_hold` stalls any AXI
  read of `s_vline`, so a fill can never capture memory made stale by the
  victim demotions. The AXI master therefore only ever sees the writeback
  before any post-demotion read of the line.
- SF for the victim: UPDATE ∅ (only when the compare-and-remove actually dropped
  the entry).

## 16. Common mechanics

### 16.1 Snoop issue and collection (PH_SNOOP / PH_VIC_SNP)

- Mask computed once per snoop phase from the DIR snapshot ∪ SF snapshot per
  PROTOCOL §7.1 (requester inclusion per §7.2).
- ≤1 snoop per cycle globally; each emission allocates the target port's ID, pushes the
  SNP flit into the port's TX SNP FIFO, and records (port, TxnID) → slot for steering.
- Collection: UpRSP/SnpResp (final state in Resp; any non-I answer from a would-be
  unique owner is noted) and UpDAT/SnpRespData (2 beats merged under BE; sets
  `s_gotpd`). SnpResp xor SnpRespData per target (assertion-checked).
- Phase completes when every masked target has answered. Snoop IDs are freed on
  completion of each target's response (UpRSP-side / UpDAT-side free).

### 16.2 PD data and writebacks

Any PD merge makes the buffer a full clean line image (PD BE is all-ones). The merged
image is written to AXI (`PH_AXI_WR`, full strobe) whenever the line must become clean
in memory (all read flows with PD, victim eviction, CleanShared/CleanInvalid, WriteUniquePtl
merge). Venus never forwards `*_PD` on CompData — dirty responsibility always terminates
at memory.

### 16.3 Responses (PH_RESP)

- One DnRSP or DnDAT per cycle globally, arbitrated round-robin among requesting slots.
- CompData beats: DataID 0 then 1 (or the single Size-selected beat), constant Resp,
  DBID allocated at beat 0 and echoed; Data = buffer beats; BE = all-ones.
- Comp/CompCMO/CompStash: single DnRSP, DBID where §5 requires (Comp of RU-upgrade /
  MakeUnique), else DBID=0.
- Write completions: DBIDResp first, Comp after AXI B (split form chosen for
  observability; both forms are protocol-legal).

### 16.4 Retirement (PH_DONE)

A slot frees when ALL hold: responses emitted, CompAck received (reads/dataless, may
pre-arrive and is latched), write data complete, AXI done, DIR commit done, SF update
done, and (Evict) the Comp has fired on the wire. Freeing releases the same-address CAM
entry and the slot.

### 16.5 Assertions (simulation)

- One slot per line (CAM uniqueness).
- SnpResp xor SnpRespData per snoop target.
- DBID constant across a transaction's flits.
- No Comp emitted with ExpCompData=1 (and vice versa).
- No CompData while a same-line WriteBackFull is in flight (M5 — structurally implied;
  asserted via the CAM).
- Emit muxes one-hot.
