# Quick Guide: Cache State Trackers

CHIron provides two complementary mechanisms for tracking cache coherency state:

- **`CacheStateTransition`** – a compile-time descriptor that encodes which initial cache states are legal for a given request and which response/data values are valid.
- **`RNCacheStateMap<config>`** – a runtime per-address cache state map for an RN-F node that validates every channel flit against the CHI state-machine and updates the stored state when a transaction completes.

This guide shows how to use both, together with the supporting `CacheState` and `CacheResp` types.

> **Issue coverage**: Full support for Issue E/EB. Issue B/C are partially supported.

---

## Table of Contents

1. [CacheState – representing one cache line's state](#1-cachestate--representing-one-cache-lines-state)
2. [CacheResp – extended response state](#2-cacheresp--extended-response-state)
3. [CacheStateTransition – encoding valid transitions](#3-cachestatetransition--encoding-valid-transitions)
4. [Pre-computed transition tables](#4-pre-computed-transition-tables)
5. [RNCacheStateMap – per-address state tracker](#5-rncachestatemap--per-address-state-tracker)
6. [Silent-operation flags](#6-silent-operation-flags)
7. [Combining state tracking with Joints](#7-combining-state-tracking-with-joints)
8. [Complete worked example](#8-complete-worked-example)
9. [Quick-reference: state abbreviations](#9-quick-reference-state-abbreviations)

---

## 1. CacheState – representing one cache line's state

`CacheState` is an 8-bit bit-field that records which CHI cache states are *simultaneously possible* for a cache line. Each bit corresponds to one named state.

```cpp
#include "chi_eb/xact/chi_eb_xact_state.hpp"
using namespace CHI::Eb;

// Predefined single-state constants (in namespace Xact::CacheStates)
Xact::CacheState uc  = Xact::CacheStates::UC;   // Unique Clean
Xact::CacheState sce = Xact::CacheStates::UCE;  // Unique Clean Empty
Xact::CacheState ud  = Xact::CacheStates::UD;   // Unique Dirty
Xact::CacheState udp = Xact::CacheStates::UDP;  // Unique Dirty PassDirty
Xact::CacheState sc  = Xact::CacheStates::SC;   // Shared Clean
Xact::CacheState sd  = Xact::CacheStates::SD;   // Shared Dirty
Xact::CacheState inv = Xact::CacheStates::I;    // Invalid
Xact::CacheState none = Xact::CacheStates::None; // No state (empty set)
Xact::CacheState all  = Xact::CacheStates::All;  // All states (any)
```

### Bit operations

`CacheState` supports the standard set operations through operator overloads:

```cpp
// Union – "UC or SC"
Xact::CacheState ucOrSc = Xact::CacheStates::UC | Xact::CacheStates::SC;

// Intersection
Xact::CacheState both = ucOrSc & Xact::CacheStates::UC;  // == UC

// Complement
Xact::CacheState notI = ~Xact::CacheStates::I;  // all dirty/clean states

// Test if non-empty (operator bool)
if (ucOrSc) { /* at least one bit set */ }

// Equality
assert((Xact::CacheStates::UC | Xact::CacheStates::UC) == Xact::CacheStates::UC);
```

### Constructing from a flit response field

```cpp
Xact::Resp snpRespCode = Resps::SnpResp_SC;

// Convert a SnpResp Resp field to the resulting CacheState
Xact::CacheState postSnpState = Xact::CacheState::FromSnpResp(snpRespCode);

// Similarly for CompData and DataSepResp responses (returns CacheResp, see §2)
```

### String representation

```cpp
std::cout << Xact::CacheStates::UD.ToString() << "\n";  // "UD"
```

---

## 2. CacheResp – extended response state

`CacheResp` is a 16-bit extension of `CacheState` that also tracks the **PassDirty** variants of every state. It is used to describe the *set of valid response values* for a response field rather than the current line state.

```cpp
// Predefined CacheResp constants (in namespace Xact::CacheResps)
Xact::CacheResp ucResp   = Xact::CacheResps::UC;
Xact::CacheResp udPdResp = Xact::CacheResps::UD_PD;  // UD with PassDirty
Xact::CacheResp iPdResp  = Xact::CacheResps::I_PD;   // I with PassDirty
```

### PassDirty helpers

```cpp
Xact::CacheResp resp = Xact::CacheResps::UC;

// Get the PassDirty twin of a response set
Xact::CacheResp pdVariant    = resp.PD();     // UC_PD
// Strip PassDirty
Xact::CacheResp nonPdVariant = resp.NonPD();  // UC
```

### Constructing from flit response fields

```cpp
// CompData Resp field
Xact::CacheResp fromComp = Xact::CacheResp::FromCompData(Resps::CompData_UC);

// DataSepResp Resp field
Xact::CacheResp fromSep  = Xact::CacheResp::FromDataSepResp(Resps::DataSepResp_SC);

// CopyBackWrData Resp field
Xact::CacheResp fromCBW  = Xact::CacheResp::FromCopyBackWrData(Resps::CopyBackWrData_UD);
```

### Implicit conversion

`CacheState` can be implicitly converted to `CacheResp` (the non-PassDirty set):

```cpp
Xact::CacheResp r = Xact::CacheStates::SC;  // r == CacheResps::SC
```

---

## 3. CacheStateTransition – encoding valid transitions

`CacheStateTransition` is a compile-time descriptor that captures everything CHI says about when a request is legal and what responses it may produce. You typically read these records from the pre-computed tables (§4) rather than constructing them by hand, but understanding the structure helps with debugging.

### Key fields

| Field | Type | Description |
|---|---|---|
| `type` | `CacheStateTransition::Type` | Category of transition (Read, Dataless, WriteCopyBack, Snoop, …) |
| `initialExpectedState` | `CacheState` | The single "expected" initial state (must-match state) |
| `initialPermittedState` | `CacheState` | Additional initial states that are also valid |
| `finalState` | `CacheState` | Cache state after the transaction completes |
| `respCompData` | `CacheResp` | Set of valid `Resp` values in a CompData flit |
| `respDataSepResp` | `CacheResp` | Set of valid `Resp` values in a DataSepResp flit |
| `respComp` | `CacheResp` | Set of valid `Resp` values in a Comp flit |
| `respCopyBackWrData` | `CacheResp` | Set of valid `Resp` values in CopyBackWrData |
| `respSnpResp` | `CacheResp` | Set of valid `Resp` values in SnpResp |
| `respSnpRespData` | `CacheResp` | Set of valid `Resp` values in SnpRespData |
| `retToSrc` | `RetToSrc` | Which RetToSrc options are valid |
| `dataPull` | `bool` | Whether a data pull is involved |
| `noChange` | `bool` | If true, cache state is unchanged regardless of response |

### Fluent builder interface

`CacheStateTransition` supports a fluent (chained) construction pattern for defining custom transitions:

```cpp
using namespace Xact;

// Example: describe ReadClean I -> UC
CacheStateTransition t =
    CacheStateTransition(CacheStates::I, CacheStates::UC)  // initial, final
        .TypeRead()
        .CompData(CacheResps::UC | CacheResps::SC)
        .DataSepResp(CacheResps::UC | CacheResps::SC);

// Get the full set of permitted initial states
CacheState allInitials = t.GetPermittedInitials();  // I
```

---

## 4. Pre-computed transition tables

CHIron ships with a complete set of pre-computed `CacheStateTransition` arrays under the `Xact::CacheStateTransitions` namespace. These are evaluated at compile time and cover every CHI Issue E opcode.

### Namespace layout

```
Xact::CacheStateTransitions::
  Initials::          – CacheState: set of valid initial states per opcode
  Transitions::       – std::array of CacheStateTransition per opcode
  Intermediates::     – nested transitions (for two-step protocols like DMT/DCT)
  Responses::         – response validation tables
```

### Reading permitted initial states

```cpp
// Is CacheStates::UC a valid initial state for ReadShared?
Xact::CacheState validInitials = Xact::CacheStateTransitions::Initials::ReadShared;
bool ok = (validInitials & Xact::CacheStates::UC) != Xact::CacheStates::None;
```

### Iterating over all transitions for one opcode

```cpp
// All valid transitions for ReadClean (Transfer variant)
for (const auto& t : Xact::CacheStateTransitions::Transitions::ReadClean_Transfer) {
    std::cout << "Initial: " << t.initialExpectedState.ToString()
              << " -> Final: " << t.finalState.ToString() << "\n";
}
```

### Available opcode groups (selection)

**Read**: `ReadNoSnp`, `ReadOnce`, `ReadClean_Transfer`, `ReadShared_Transfer`, `ReadUnique_Transfer`, `ReadNotSharedDirty_Transfer`, `MakeReadUnique_Transfer`, …

**Dataless**: `CleanUnique`, `MakeUnique`, `CleanShared`, `CleanInvalid`, `MakeInvalid`, `Evict`, `CleanSharedPersistSep`, …

**Write (CopyBack)**: `WriteBackFull`, `WriteCleanFull`, `WriteEvictFull`, `WriteEvictOrEvict`, …

**Write (Immediate)**: `WriteNoSnpPtl`, `WriteNoSnpFull`, `WriteUniquePtl`, `WriteUniqueFull`, …

**Atomic**: `AtomicStore`, `AtomicLoad`, `AtomicSwap`, `AtomicCompare`, …

**Snoop**: `SnpOnce`, `SnpClean`, `SnpShared`, `SnpNotSharedDirty`, `SnpUnique`, `SnpMakeInvalid`, `SnpCleanShared`, `SnpCleanInvalid`, `SnpPreferUnique`, …

**Forward Snoop**: `SnpOnceFwd`, `SnpCleanFwd`, `SnpSharedFwd`, `SnpNotSharedDirtyFwd`, `SnpUniqueFwd`, `SnpPreferUniqueFwd`, …

---

## 5. RNCacheStateMap – per-address state tracker

`RNCacheStateMap<config>` is the main runtime state tracker for an RN-F node. It maintains a per-address cache state and validates every flit against the CHI protocol state machine.

### Construction

```cpp
#include "chi_eb/xact/chi_eb_xact_state.hpp"
using namespace CHI::Eb;

Xact::RNCacheStateMap<config> stateMap;

// Optionally pre-seed a known state (e.g. warm cache after reset)
stateMap.Set(0x0000'1000, Xact::CacheStates::UC);
```

### Feeding flits

Call `Next*` for each flit in channel order. The map validates the flit against the current state and returns an `XactDenialEnum`.

```cpp
Xact::Global<config>  glbl;
Xact::Xaction<config>* xaction = /* pointer obtained from RNFJoint */;

Xact::XactDenialEnum denial;

// TX REQ – validate that the current state allows this request
denial = stateMap.NextTXREQ(addr, req);

// RX SNP – a snoop arrived; check legality given current state
denial = stateMap.NextRXSNP(addr, snp);

// TX RSP – send SnpResp/CompAck
denial = stateMap.NextTXRSP(addr, *xaction, rsp);

// TX DAT – send CopyBackWrData / SnpRespData
denial = stateMap.NextTXDAT(addr, *xaction, dat);

// RX RSP – receive Comp / DBIDResp
denial = stateMap.NextRXRSP(addr, *xaction, rsp);

// RX DAT – receive CompData / DataSepResp
denial = stateMap.NextRXDAT(addr, *xaction, dat);
```

> **Note**: `Next*RSP` and `Next*DAT` require the associated `Xaction` object (obtainable from `RNFJoint`) so the map can resolve which transfer protocol (DMT, DCT, DWT, etc.) is in progress.

### Querying the current state

```cpp
// Get the stored state (does not apply silent-op heuristics)
Xact::CacheState stored = stateMap.Get(0x0000'1000);

// Evaluate the effective state (applies silent eviction/sharing heuristics)
Xact::CacheState effective = stateMap.Evaluate(0x0000'1000);

// Excavate the state, taking seer mode into account
Xact::CacheState excavated = stateMap.Excavate(0x0000'1000);

std::cout << "State at 0x1000: " << effective.ToString() << "\n";
```

### Manual state transfer

Use `Transfer` to advance the state directly (e.g. after processing a snoop response from outside the map):

```cpp
// Force the cache line at addr to transition to SC
denial = stateMap.Transfer(addr, Xact::CacheStates::SC);
```

---

## 6. Silent-operation flags

CHI permits requesters to silently evict, share, store to, or invalidate a cache line without issuing a transaction. `RNCacheStateMap` models these via four flags (all **enabled** by default):

| Flag | Setter | Description |
|---|---|---|
| Silent Eviction | `SetSilentEviction(bool)` | Line may silently transition from any state to I |
| Silent Sharing | `SetSilentSharing(bool)` | Line may silently downgrade from UC/UD to SC/SD |
| Silent Store | `SetSilentStore(bool)` | Line may silently become UD from UC |
| Silent Invalidation | `SetSilentInvalidation(bool)` | Line may silently become I from SC |

```cpp
// Disable all silent operations for strict protocol checking
stateMap.SetSilentEviction(false);
stateMap.SetSilentSharing(false);
stateMap.SetSilentStore(false);
stateMap.SetSilentInvalidation(false);

// Re-query flags
bool canEvict = stateMap.HasSilentEviction();  // false
```

---

## 7. Combining state tracking with Joints

For RN-F level monitoring, feed **both** the `RNFJoint` and `RNCacheStateMap` with each flit. The joint manages transaction identity and completeness; the state map manages the per-address coherency state.

```cpp
Xact::Global<config>      glbl;
Xact::RNFJoint<config>    joint;
Xact::RNCacheStateMap<config> stateMap;

uint64_t addr = req.Addr;
std::shared_ptr<Xact::Xaction<config>> xaction;
Xact::XactDenialEnum denial;

// ── TXREQ ────────────────────────────────────────────────────────────────────
denial = joint.NextTXREQ(glbl, time, req, &xaction);
if (denial == Xact::XactDenial::ACCEPTED)
    stateMap.NextTXREQ(addr, req);

// ── RXDAT (CompData) ─────────────────────────────────────────────────────────
denial = joint.NextRXDAT(glbl, time, dat, &xaction);
if (denial == Xact::XactDenial::ACCEPTED && xaction)
    stateMap.NextRXDAT(addr, *xaction, dat);

// ── TXRSP (CompAck) ──────────────────────────────────────────────────────────
denial = joint.NextTXRSP(glbl, time, rsp, &xaction);
if (denial == Xact::XactDenial::ACCEPTED && xaction)
    stateMap.NextTXRSP(addr, *xaction, rsp);

// After the transaction is complete:
std::cout << "Post-transaction state: "
          << stateMap.Evaluate(addr).ToString() << "\n";
```

---

## 8. Complete worked example

The example below traces a ReadClean (I→UC) followed by an Evict on the same address.

```cpp
#define CHI_ISSUE_EB_ENABLE
#include "chi_eb/xact/chi_eb_xact_state.hpp"
#include "chi_eb/xact/chi_eb_joint.hpp"

#include <iostream>
#include <cassert>

using namespace CHI::Eb;
using config = FlitConfiguration<7, 48, 0, 0, 256, false, false, false>;

int main()
{
    // ── Setup ─────────────────────────────────────────────────────────────────
    Xact::Global<config>          glbl;
    Xact::RNFJoint<config>        joint;
    Xact::RNCacheStateMap<config> states;

    glbl.TOPOLOGY.SetNode(0x01, Xact::NodeType::RN_F, "cpu0");
    glbl.TOPOLOGY.SetNode(0x10, Xact::NodeType::HN_F, "hnf0");
    glbl.CHECK_FIELD_MAPPING.enable = false;

    const uint64_t ADDR = 0x0000'4000;
    std::shared_ptr<Xact::Xaction<config>> xact;
    Xact::XactDenialEnum denial;

    // ──────────────────────────────────────────────────────────────────────────
    // Transaction 1: ReadClean  I -> UC
    // ──────────────────────────────────────────────────────────────────────────

    // (a) TXREQ: ReadClean
    Flits::REQ<config> req{};
    req.Opcode = Opcodes::REQ::ReadClean;
    req.SrcID  = 0x01;  req.TxnID = 1;
    req.Addr   = ADDR;  req.Size  = 6;

    denial = joint.NextTXREQ(glbl, 100, req, &xact);
    assert(denial == Xact::XactDenial::ACCEPTED);
    states.NextTXREQ(ADDR, req);

    std::cout << "After ReadClean REQ : " << states.Evaluate(ADDR).ToString() << "\n";
    // -> "I" (still invalid; data not yet received)

    // (b) RXDAT: CompData(UC)
    Flits::DAT<config> dat{};
    dat.Opcode = Opcodes::DAT::CompData;
    dat.TxnID  = 1;  dat.SrcID = 0x10;  dat.TgtID = 0x01;
    dat.Resp   = Resps::CompData_UC;  dat.DBID = 0;

    denial = joint.NextRXDAT(glbl, 105, dat, &xact);
    assert(denial == Xact::XactDenial::ACCEPTED);
    if (xact) states.NextRXDAT(ADDR, *xact, dat);

    // (c) TXRSP: CompAck
    Flits::RSP<config> rsp{};
    rsp.Opcode = Opcodes::RSP::CompAck;
    rsp.TxnID  = 0;  rsp.SrcID = 0x01;  rsp.TgtID = 0x10;

    denial = joint.NextTXRSP(glbl, 106, rsp, &xact);
    assert(denial == Xact::XactDenial::ACCEPTED);
    if (xact) states.NextTXRSP(ADDR, *xact, rsp);

    std::cout << "After ReadClean done: " << states.Evaluate(ADDR).ToString() << "\n";
    // -> "UC"

    // ──────────────────────────────────────────────────────────────────────────
    // Transaction 2: Evict  UC -> I
    // ──────────────────────────────────────────────────────────────────────────

    req = Flits::REQ<config>{};
    req.Opcode = Opcodes::REQ::Evict;
    req.SrcID  = 0x01;  req.TxnID = 2;
    req.Addr   = ADDR;

    denial = joint.NextTXREQ(glbl, 200, req, &xact);
    assert(denial == Xact::XactDenial::ACCEPTED);
    states.NextTXREQ(ADDR, req);

    // Comp response from HN-F
    rsp = Flits::RSP<config>{};
    rsp.Opcode = Opcodes::RSP::Comp;
    rsp.TxnID  = 2;  rsp.SrcID = 0x10;  rsp.TgtID = 0x01;
    rsp.Resp   = Resps::Comp_I;

    denial = joint.NextRXRSP(glbl, 210, rsp, &xact);
    assert(denial == Xact::XactDenial::ACCEPTED);
    if (xact) states.NextRXRSP(ADDR, *xact, rsp);

    std::cout << "After Evict done    : " << states.Evaluate(ADDR).ToString() << "\n";
    // -> "I"

    return 0;
}
```

Expected output:

```
After ReadClean REQ : I
After ReadClean done: UC
After Evict done    : I
```

---

## 9. Quick-reference: state abbreviations

| Abbreviation | Full name | Dirty? | Exclusive? |
|---|---|---|---|
| `UC` | Unique Clean | No | Yes |
| `UCE` | Unique Clean Empty | No | Yes (empty) |
| `UD` | Unique Dirty | Yes | Yes |
| `UDP` | Unique Dirty PassDirty | Yes | Yes |
| `SC` | Shared Clean | No | No |
| `SD` | Shared Dirty | Yes | No |
| `I` | Invalid | – | – |

PassDirty (`_PD`) variants in `CacheResp` indicate that the PassDirty bit is set in the response field, transferring responsibility for writeback from the sender to the receiver.
