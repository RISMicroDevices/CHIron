# Guide: Adding New Opcodes

This guide explains how to define and integrate a new opcode across all relevant components of CHIron.

---

## Overview

Opcodes identify the operation type of a flit on a given channel.  CHIron uses a four-channel model:

| Channel | Abbreviation | Direction | Purpose |
|---------|--------------|-----------|---------|
| Request  | REQ | RN → HN | Cache requests from requester nodes |
| Snoop    | SNP | HN → RN | Snoop requests from home nodes |
| Response | RSP | both | Control responses without data |
| Data     | DAT | both | Responses that carry data |

Every new opcode requires changes in several places, described step-by-step below.

---

## Background: multi-issue compilation model

CHIron supports multiple AMBA CHI issues (B, C, E, EB/E.b) in a single build by stacking
`#ifdef` guards.  Each source file uses the following pattern:

```cpp
#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__..._B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__..._EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__..._B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__..._EB
#endif

// ... declarations ...

#endif
```

The outer `#if` ensures that when both Issue B and Issue EB are enabled in the same build the
block is processed only once per issue, preventing duplicate symbol errors.  Keep the same pattern
in every file you touch.

---

## Step 1 — Choose the Channel and Allocate an Encoding

Consult the AMBA CHI specification to determine:

- Which channel the opcode belongs to (REQ / SNP / RSP / DAT).
- Which AMBA CHI issue introduces it (B, C, E, EB/E.b).
- What numeric encoding is assigned to it.

Reserved slots are marked with comments in `chi/spec/chi_protocol_encoding.hpp`:

```cpp
//          *Reserved*                                                  = 0x3B;
```

Do **not** reuse a reserved slot without full protocol compliance.

---

## Step 2 — Declare the Opcode Constant

**File:** `chi/spec/chi_protocol_encoding.hpp`

Opcode constants are declared inside the matching channel namespace under `namespace Opcodes`.  The `type` alias for each channel is derived from the corresponding flit structure.

### Base-issue (Issue E) opcode

```cpp
namespace Opcodes {
    namespace REQ {
        // existing declarations …

        constexpr type MyNewOpcode = 0xXX;  // chosen encoding
    }
}
```

Replace `REQ` with `SNP`, `RSP`, or `DAT` as appropriate.

### Issue EB (Early Bird) opcode

Wrap the declaration in the corresponding preprocessor guard:

```cpp
#ifdef CHI_ISSUE_EB_ENABLE
    constexpr type MyNewOpcode = 0xXX;
#endif
```

Issue B (`CHI_ISSUE_B_ENABLE`) and Issue C (`CHI_ISSUE_C_ENABLE`) follow the same pattern.

### Opcode encoding width

The opcode field widths (and therefore the number of valid slots) are determined by the flit type:

| Channel | Opcode type alias | Width |
|---------|-------------------|-------|
| REQ | `Flits::REQ<>::opcode_t` | 7 bits (128 slots, Issue EB: 7 bits = 128 slots) |
| SNP | `Flits::SNP<>::opcode_t` | 5 bits (32 slots) |
| RSP | `Flits::RSP<>::opcode_t` | 5 bits (32 slots) |
| DAT | `Flits::DAT<>::opcode_t` | 4 bits (16 slots) |

Encodings must fit within these widths.  The `type` alias at the top of each channel namespace
(`using type = Flits::REQ<>::opcode_t;`) is the correct integer type to use for all constants.

---

## Step 3 — Add Node-Type Namespace Aliases (REQ and SNP only)

Some opcodes are valid only for specific node types.  The `chi/spec/chi_protocol_encoding.hpp` file contains per-node-type sub-namespaces that re-export the subset of valid opcodes.  Sub-namespaces for REQ:

```
Opcodes::REQ::RN     – opcodes usable by any RN
Opcodes::REQ::RN_F   – opcodes usable by RN-F (full-cache requester)
Opcodes::REQ::RN_D   – opcodes usable by RN-D (DVM-only requester)
Opcodes::REQ::HN     – opcodes usable by HN (forward/subordinate requests)
```

If your opcode should appear in one or more of these restricted sets, add a constant alias inside the matching sub-namespace:

```cpp
namespace Opcodes {
    namespace REQ {
        namespace RN {
            constexpr type MyNewOpcode = REQ::MyNewOpcode;
        }
        namespace RN_F {
            constexpr type MyNewOpcode = REQ::MyNewOpcode;
        }
    }
}
```

For SNP opcodes the sub-namespaces are:

```
Opcodes::SNP::RN     – snoops receivable by any RN
Opcodes::SNP::RN_F   – snoops receivable by RN-F
```

---

## Step 4 — Define the Field Mapping

Each opcode has a *field mapping* that records which flit fields are applicable for that opcode and what constraints apply to each field.

**File:** `chi/xact/chi_xact_field_eb.hpp` (for Issue B: `chi/xact/chi_xact_field_b.hpp` — same structure)

### 4a — Understand the `FieldConvention` codes

```cpp
enum class FieldConvention {
    A0  = 0,  // Applicable. Must be zero.
    A1  = 1,  // Applicable. Must be one.
    I0,       // Inapplicable. Must be zero.
    X,        // Inapplicable. Can take any value.
    Y,        // Applicable. Specification constrained.
    B8,       // Size field must encode 8 bytes.
    B64,      // Size field must encode 64 bytes (cache-line).
    S,        // Assigned to another shared field (overlapping encodings).
    D         // Inapplicable. Must be default value of MPAM field.
};
```

The field checker in `chi/xact/chi_xact_checkers_field.hpp` enforces `A0` and `A1` at runtime; all other conventions are informational constraints validated by external tooling or ignored.

### 4b — Add a field-mapping constant in the appropriate `*FieldMappings` namespace

#### REQ opcode

The `RequestFieldMappingBack` constructor takes arguments in this order:

```
QoS, TgtID, SrcID, TxnID, Opcode, AllowRetry, PCrdType, RSVDC,
TagOp, TraceTag, MPAM, Addr, NS, Size, Allocate, Cacheable, Device,
EWA, SnpAttr, DoDWT, Order, LikelyShared, Excl, SnoopMe, ExpCompAck,
LPID, TagGroupID, StashGroupID, PGroupID, ReturnNID, StashNID,
SLCRepHint, StashNIDValid, Endian, Deep, ReturnTxnID, StashLPIDValid, StashLPID
```

Example:

```cpp
namespace RequestFieldMappings {
    using enum FieldConvention;
    //  QoS  TgtID SrcID TxnID Op   AllowR PCrd RSVDC TagOp Trace MPAM Addr NS   Size  Alloc Cache Dev  EWA  SnpA DoDWT Order LkSh Excl SnpMe ExpCA LPID TGrp SGrp PGrp RetNID StNID SLCHint StNIDV Endian Deep RetTID StLPIDV StLPID
    inline constexpr RequestFieldMappingBack MyReadOpcode
        (Y ,  Y ,  Y ,  Y ,  Y ,  Y ,  Y ,  Y ,  Y ,  Y ,  Y ,  Y ,  Y ,  B64, Y ,  Y ,  Y ,  Y ,  Y ,  S ,  Y ,  Y ,  Y ,  S ,  A1,  Y ,  S ,  S ,  S ,  S ,  S ,  Y ,  I0,  I0,  I0,  I0,  I0,  I0);
}
```

Use existing opcodes in `RequestFieldMappings` as templates; change only the fields that differ from the nearest similar opcode.

#### RSP opcode

The `ResponseFieldMappingBack` constructor takes arguments in this order:

```
QoS, TgtID, SrcID, TxnID, Opcode, RespErr, Resp, CBusy, DBID,
TagGroupID, StashGroupID, PGroupID, PCrdType, FwdState, DataPull, TagOp, TraceTag
```

Example:

```cpp
namespace ResponseFieldMappings {
    using enum FieldConvention;
    //  QoS TgtID SrcID TxnID Op   RespErr Resp CBusy DBID TGrp SGrp PGrp PCrd FwdSt DtPull TagOp Trace
    inline const ResponseFieldMappingBack MyNewRSPOpcode
        (Y , Y ,   Y ,   Y ,   Y ,  Y ,   Y ,  Y ,   Y ,  S ,  S ,  S ,  I0,  I0,   I0,    Y ,   Y );
}
```

#### DAT opcode

The `DataFieldMappingBack` constructor takes arguments in this order:

```
QoS, TgtID, SrcID, TxnID, Opcode, RespErr, Resp, CBusy, DBID,
CCID, DataID, RSVDC, BE, Data, HomeNID, FwdState, DataPull, DataSource,
TraceTag, DataCheck, Poison, TagOp, Tag, TU
```

Example:

```cpp
namespace DataFieldMappings {
    using enum FieldConvention;
    //  QoS TgtID SrcID TxnID Op  RespErr Resp CBusy DBID CCID DataID RSVDC BE Data HomeNID FwdSt DtPull DtSrc Trace DtChk Poison TagOp Tag TU
    inline constexpr DataFieldMappingBack MyNewDATOpcode
        (Y , Y ,   Y ,   Y ,   Y , Y ,   Y ,  Y ,   Y ,  Y ,  Y ,   Y ,   X , Y ,  Y ,    S ,   S ,    Y ,   Y ,   Y ,   Y ,   Y ,   Y , Y );
}
```

#### SNP opcode

The `SnoopFieldMappingBack` constructor takes arguments in this order:

```
QoS, SrcID, TxnID, Opcode, Addr, NS, FwdNID, FwdTxnID,
StashLPIDValid, StashLPID, VMIDExt, DoNotGoToSD, RetToSrc, TraceTag, MPAM
```

Example:

```cpp
namespace SnoopFieldMappings {
    using enum FieldConvention;
    //  QoS SrcID TxnID Op  Addr NS  FwdNID FwdTxnID StLPIDV StLPID VMIDExt DoNotGoSD RetToSrc Trace MPAM
    inline constexpr SnoopFieldMappingBack MyNewSNPOpcode
        (Y , Y ,   Y ,   Y , Y ,  Y , I0,    I0,      I0,     I0,    I0,     A1,       I0,      Y ,   D );
}
```

### 4c — Register the mapping in the constructor of the mapping table

Locate the constructor implementation in the same file and add one line in the appropriate section:

```cpp
// In RequestFieldMappingTable::RequestFieldMappingTable() noexcept
table[Opcodes::REQ::MyNewOpcode] = &RequestFieldMappings::MyNewOpcode;
```

Or for the other channels:

```cpp
// In ResponseFieldMappingTable::ResponseFieldMappingTable() noexcept
table[Opcodes::RSP::MyNewRSPOpcode] = &ResponseFieldMappings::MyNewRSPOpcode;

// In DataFieldMappingTable::DataFieldMappingTable() noexcept
table[Opcodes::DAT::MyNewDATOpcode] = &DataFieldMappings::MyNewDATOpcode;

// In SnoopFieldMappingTable::SnoopFieldMappingTable() noexcept
table[Opcodes::SNP::MyNewSNPOpcode] = &SnoopFieldMappings::MyNewSNPOpcode;
```

Wrap in the appropriate `#ifdef` block when the opcode is issue-specific:

```cpp
#ifdef CHI_ISSUE_EB_ENABLE
    table[Opcodes::REQ::MyNewOpcode] = &RequestFieldMappings::MyNewOpcode;
#endif
```

> **Important:** entries not added to the table are implicitly `nullptr`, which causes the field
> checker to skip validation for that opcode.  Always add an entry.

---

## Step 5 — Integrate with Transaction Implementations

An opcode that starts a new transaction type needs to be recognised by a `Xaction*` constructor.  An opcode that is a *response* to an existing transaction type needs to be handled inside `NextRSPNoRecord()` or `NextDATNoRecord()`.

### REQ opcode → existing transaction class mapping

The table below shows which `Xaction*` class each REQ opcode category maps to.  Use it to decide which constructor to modify.

| Xaction class | Representative REQ opcodes |
|---|---|
| `XactionAllocatingRead` | ReadShared, ReadClean, ReadUnique, ReadNotSharedDirty, ReadPreferUnique, MakeReadUnique |
| `XactionNonAllocatingRead` | ReadOnce, ReadNoSnp, ReadOnceCleanInvalid, ReadOnceMakeInvalid |
| `XactionImmediateWrite` | WriteUniquePtl, WriteUniqueFull, WriteNoSnpPtl, WriteNoSnpFull, WriteUniqueFullStash, WriteUniquePtlStash |
| `XactionWriteZero` | WriteUniqueZero, WriteNoSnpZero |
| `XactionCopyBackWrite` | WriteEvictFull, WriteCleanFull, WriteBackPtl, WriteBackFull, WriteEvictOrEvict |
| `XactionAtomic` | AtomicStore/Load/Swap/Compare (all sub-opcodes) |
| `XactionIndependentStash` | StashOnceShared, StashOnceUnique, StashOnceSepShared, StashOnceSepUnique |
| `XactionDataless` | CleanShared, CleanInvalid, MakeInvalid, CleanUnique, MakeUnique, Evict, CleanSharedPersist, CleanSharedPersistSep |

### 5a — Associating a REQ opcode with an existing transaction class

Open the constructor of the relevant transaction implementation (e.g., `chi/xact/chi_xactions/chi_xactions_impl_Dataless.hpp`) and add the opcode to the whitelist check:

```cpp
if (
    this->first.flit.req.Opcode() != Opcodes::REQ::ExistingOpcode1
 && this->first.flit.req.Opcode() != Opcodes::REQ::ExistingOpcode2
 // …
#ifdef CHI_ISSUE_EB_ENABLE
 && this->first.flit.req.Opcode() != Opcodes::REQ::MyNewOpcode
#endif
) [[unlikely]]
{
    this->firstDenial = XactDenial::DENIED_REQ_OPCODE;
    return;
}
```

### 5b — Handling a new RSP or DAT opcode in `NextRSPNoRecord` / `NextDATNoRecord`

These virtual methods are called when the transaction absorbs a subsequent flit.  The caller (`RNFJoint`) calls `xaction->NextRSP(glbl, ...)` which internally calls `NextRSPNoRecord` and, on `ACCEPTED`, appends the flit to `this->subsequence`.

Return `XactDenial::ACCEPTED` on success or one of the `XactDenial::DENIED_*` constants below on failure.

**Common denial codes for RSP handling:**

| Denial code | When to use |
|---|---|
| `DENIED_COMPLETED_RSP` | Transaction already complete — no more RSP flits expected |
| `DENIED_CHANNEL_NOT_RSP` | Flit is not on the RSP channel |
| `DENIED_RSP_OPCODE` | RSP opcode not expected by this transaction |
| `DENIED_RSP_NOT_FROM_HN_TO_RN` | Direction check failed (should be HN→RN) |
| `DENIED_RSP_NOT_FROM_RN_TO_HN` | Direction check failed (should be RN→HN, e.g. CompAck) |
| `DENIED_RSP_TXNID_MISMATCHING_REQ` | TxnID in RSP does not match the REQ TxnID |
| `DENIED_RSP_TGTID_MISMATCHING_REQ` | TgtID in RSP does not match the expected target |
| `DENIED_COMP_AFTER_COMP` | Duplicate Comp response received |
| `DENIED_COMPACK_AFTER_COMPACK` | Duplicate CompAck received |
| `DENIED_COMPACK_BEFORE_COMP` | CompAck arrived before Comp |

**Common denial codes for DAT handling:**

| Denial code | When to use |
|---|---|
| `DENIED_COMPLETED_DAT` | Transaction already complete |
| `DENIED_CHANNEL_NOT_DAT` | Flit is not on the DAT channel |
| `DENIED_DAT_OPCODE` | DAT opcode not expected |
| `DENIED_DAT_NOT_FROM_HN_TO_RN` | Direction check failed |
| `DENIED_DAT_NOT_FROM_RN_TO_HN` | Direction check failed for RN-sourced data |
| `DENIED_DAT_TXNID_MISMATCHING_REQ` | TxnID mismatch |

Full skeleton with the standard RetryAck/PCrdGrant pattern and one custom opcode:

```cpp
virtual XactDenialEnum NextRSPNoRecord(
    const Global<config>& glbl,
    const FiredResponseFlit<config>& rspFlit,
    bool& hasDBID, bool& firstDBID) noexcept override
{
    if (IsComplete(glbl))
        return XactDenial::DENIED_COMPLETED_RSP;

    if (!rspFlit.IsRSP())
        return XactDenial::DENIED_CHANNEL_NOT_RSP;

    // RetryAck and PCrdGrant are handled uniformly by all non-snoop transactions.
    if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
    {
        // AllowRetry enforcement is left to the external scoreboard.
        hasDBID = false;
        return XactDenial::ACCEPTED;
    }

    if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::PCrdGrant)
    {
        hasDBID = false;
        return XactDenial::ACCEPTED;
    }

    // Transaction-specific response handling.
    if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
    {
        if (!rspFlit.IsToRequester(glbl))
            return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

        if (this->HasRSP({ Opcodes::RSP::Comp }))
            return XactDenial::DENIED_COMP_AFTER_COMP;

        if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
            return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

        hasDBID   = (rspFlit.flit.rsp.DBID() != 0);
        firstDBID = hasDBID && !this->GotDBID();
        return XactDenial::ACCEPTED;
    }

#ifdef CHI_ISSUE_EB_ENABLE
    if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::MyNewRSPOpcode)
    {
        if (!rspFlit.IsToRequester(glbl))
            return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;
        hasDBID = false;
        return XactDenial::ACCEPTED;
    }
#endif

    return XactDenial::DENIED_RSP_OPCODE;
}
```

---

## Step 6 — Define or Update Cache State Transitions

If the new opcode affects cache coherence state, update the relevant cache state transition table.

**Files:**

| Opcode category | File |
|---|---|
| Read requests | `chi/xact/chi_xact_state/cst_xact_read.hpp` |
| Dataless requests | `chi/xact/chi_xact_state/cst_xact_dataless.hpp` |
| Write requests | `chi/xact/chi_xact_state/cst_xact_write.hpp` |
| Atomic requests | `chi/xact/chi_xact_state/cst_xact_atomic.hpp` |
| Snoop requests | `chi/xact/chi_xact_state/cst_xact_snoop.hpp` |

### 6a — Define the transition entry

A `CacheStateTransition` describes:
- `initialExpectedState` — the cache state the requestor is **expected** to hold before the request.
- `initialPermittedState` — additional states also **allowed** (silent transitions).
- `finalState` — the cache state after the transaction completes.
- Response fields (`respCompData`, `respDataSepResp`, etc.) — bit-masks of valid `Resp` encodings.

```cpp
namespace Transitions {

    // MyNewOpcode: I → UC after CompData_UC
    constexpr CacheStateTransition MyNewOpcode_I_to_UC = {
        CacheStateTransition(I, UC).TypeRead()
            .CompData(UC)
            .DataSepResp(UC)
    };

    // Multiple initial states combined with |
    constexpr CacheStateTransition MyNewOpcode_SC_SD_to_SC = {
        CacheStateTransition(SC | SD, SC).TypeRead()
            .CompData(SC | UC)
            .DataSepResp(SC | UC)
    };

    constexpr std::array MyNewOpcode = {
        MyNewOpcode_I_to_UC,
        MyNewOpcode_SC_SD_to_SC
    };
}
```

**`CacheStateTransition` builder methods:**

| Method | Purpose |
|---|---|
| `.TypeRead()` | Read-type transition |
| `.TypeDataless()` | Dataless-type transition |
| `.TypeMakeReadUnique()` | MakeReadUnique-specific variant |
| `.TypeWriteCopyBack()` | Copy-back write |
| `.TypeWriteNonCopyBack()` | Non-copy-back write |
| `.TypeAtomic()` | Atomic operation |
| `.TypeSnoop()` | Non-forwarding snoop |
| `.TypeSnoopForward()` | Forwarding snoop |
| `.CompData(mask)` | Valid `CacheResp` masks after CompData |
| `.DataSepResp(mask)` | Valid masks after DataSepResp |
| `.Comp(mask)` | Valid masks after Comp |
| `.CopyBackWrData(mask)` | Valid masks for CopyBackWrData |
| `.SnpResp(mask)` | Valid masks after SnpResp |
| `.SnpRespData(mask)` | Valid masks after SnpRespData |
| `.SnpRespDataPtl(mask)` | Valid masks after SnpRespDataPtl |
| `.RetToSrc(rts)` | `RetToSrc` field expectation (use `RetToSrcs::X/A0/A1`) |
| `.DataPull(dp)` | `DataPull` expectation |
| `.NoChange()` | Cache state does not change regardless of final Resp |

**Cache state shortcuts (in scope inside `cst_xact_*.hpp`):**

```
UC   – Unique Clean      UCE  – Unique Clean Exclusive
UD   – Unique Dirty      UDP  – Unique Dirty Persist
SC   – Shared Clean      SD   – Shared Dirty
I    – Invalid
```

`CacheResp` additionally has `_PD` (Pass-Dirty) variants: `UC_PD`, `SC_PD`, `UD_PD`, etc.
Combine masks with `|`: `.CompData(UC | SC)`.

### 6b — Register in `cst_consteval_initials.hpp`

**File:** `chi/xact/chi_xact_state/cst_consteval_initials.hpp`

Add a line inside the `namespace Initials { … }` block using the appropriate macro:

```cpp
// General (read / dataless / snoop) — uses both expected + permitted:
constexpr CacheState MyNewOpcode = _MR_General(MyNewOpcode);

// Write — uses expected state only:
constexpr CacheState MyNewOpcode = _MR_Write(MyNewOpcode);

// MakeReadUnique variant — uses expected state only:
constexpr CacheState MyNewOpcode = _MR_MakeReadUnique(MyNewOpcode);
```

This exposes `CacheStateTransitions::Initials::MyNewOpcode`, which is a bitmask of all valid
initial cache states for the opcode.  External validators query this value to check whether the
requester's current cache state is legal for the request.

### 6c — Register in `cst_consteval_responses.hpp` (forward snoops only)

**File:** `chi/xact/chi_xact_state/cst_consteval_responses.hpp`

Only *forwarding snoop* opcodes need an entry here.  Add:

```cpp
constexpr details::TableR0 MyNewSnpFwdOpcode = _Table_DCT(MyNewSnpFwdOpcode);
```

This builds a compile-time lookup table (`TableR0`) mapping each possible initial cache state to
the set of valid `CompData` responses that the forwarded snoop may carry.

### 6d — Register in `cst_consteval_intermediates.hpp` (snoops with intermediate state)

**File:** `chi/xact/chi_xact_state/cst_consteval_intermediates.hpp`

Snoops that have a *nested intermediate state* (e.g., forwarding snoops that involve an
intermediate transfer step) need an entry in `namespace Intermediates { namespace Nested { … } }`.
Look at the existing snoop entries in that file and follow the same pattern using the
`_Table_G0`, `_Table_G1`, and `_Table_G2` macros for your opcode's transition array.

---

## Step 7 — Register the New Opcode in the `RNFJoint` Decoder

**File:** `chi/xact/chi_joint.hpp`

The `RNFJoint` constructor maps each opcode to a factory method via two macros:

```cpp
#define SET_REQ_XACTION(opcode, type) \
    reqDecoder[Opcodes::REQ::opcode].SetCompanion(&RNFJoint<config>::Construct##type)

#define SET_SNP_XACTION(opcode, type) \
    snpDecoder[Opcodes::SNP::opcode].SetCompanion(&RNFJoint<config>::Construct##type)
```

The `type` suffix must match one of the existing `Construct*` factory methods, or `None` if the
opcode starts no transaction (e.g., credit returns):

```cpp
// Inside RNFJoint<config>::RNFJoint() noexcept — in the REQ decoder section:
SET_REQ_XACTION(MyNewOpcode, Dataless);       // uses XactionDataless

// Or for a completely new transaction type:
SET_REQ_XACTION(MyNewOpcode, MyNewTransaction); // requires ConstructMyNewTransaction()
```

For SNP opcodes, choose between `HomeSnoop` and `ForwardSnoop`:

```cpp
SET_SNP_XACTION(MyNewSNPOpcode, HomeSnoop);     // XactionHomeSnoop
SET_SNP_XACTION(MyNewSNPFwdOpcode, ForwardSnoop); // XactionForwardSnoop
```

---

## Step 8 — Handle Issue-Specific Overrides in `chi_b/`, `chi_c/`, `chi_eb/`

If the opcode only exists in a specific issue and the encoding file in `chi/spec/` does not already include it under the appropriate `#ifdef`, check whether a per-issue override file is more appropriate.  The directories `chi_b/`, `chi_c/`, and `chi_eb/` each contain small header files that override or extend definitions from the base `chi/` directory.

---

## Checklist

- [ ] Opcode constant declared in `chi/spec/chi_protocol_encoding.hpp` with correct numeric value and `#ifdef` guard.
- [ ] Opcode fits within the channel's encoding width (7-bit REQ, 5-bit SNP/RSP, 4-bit DAT).
- [ ] Node-type alias added to the relevant sub-namespaces (`RN`, `RN_F`, `HN`, …) if applicable.
- [ ] Field mapping constant added to the appropriate `*FieldMappings` namespace in `chi_xact_field_eb.hpp`.
- [ ] Field mapping registered in the `*FieldMappingTable` constructor.
- [ ] Opcode recognised in the constructor of the relevant `Xaction*` class (for REQ/SNP first-flits).
- [ ] `NextRSPNoRecord` / `NextDATNoRecord` updated (for RSP/DAT subsequent flits) with correct denial codes.
- [ ] Cache state transition array defined in the appropriate `cst_xact_*.hpp`.
- [ ] Transition registered in `cst_consteval_initials.hpp` with `_MR_General` / `_MR_Write`.
- [ ] Transition registered in `cst_consteval_responses.hpp` with `_Table_DCT` (forwarding snoops only).
- [ ] Transition registered in `cst_consteval_intermediates.hpp` (snoops with intermediate state only).
- [ ] `SET_REQ_XACTION` / `SET_SNP_XACTION` entry added in `RNFJoint` constructor in `chi_joint.hpp`.
- [ ] All changes wrapped in the appropriate issue-version `#ifdef` guard.
