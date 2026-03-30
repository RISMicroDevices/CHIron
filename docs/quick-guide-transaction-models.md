# Quick Guide: Transaction Models (Xactions)

CHIron models every CHI protocol exchange as a **transaction** (`Xaction`). A transaction begins when a request flit fires on the REQ or SNP channel and closes when all required responses and data flits have been received. This guide shows how to configure the library, feed flits into a **Joint**, react to events, and inspect completed transactions.

> **Issue coverage**: Transaction modeling currently fully supports Issue E/EB. Issue B/C have partial support.

---

## Table of Contents

1. [Concepts](#1-concepts)
2. [Setup: FlitConfiguration](#2-setup-flitconfiguration)
3. [Setup: Global and Topology](#3-setup-global-and-topology)
4. [Transaction Types](#4-transaction-types)
5. [Joints – the entry point](#5-joints--the-entry-point)
6. [Feeding flits and reading denials](#6-feeding-flits-and-reading-denials)
7. [Event callbacks](#7-event-callbacks)
8. [Inspecting a completed Xaction](#8-inspecting-a-completed-xaction)
9. [Retry and resend handling](#9-retry-and-resend-handling)
10. [Using the Companion to attach custom data](#10-using-the-companion-to-attach-custom-data)
11. [Complete worked example](#11-complete-worked-example)
12. [Common pitfalls](#12-common-pitfalls)

---

## 1. Concepts

| Term | Description |
|---|---|
| `Xaction<config>` | A single CHI transaction (one REQ/SNP flit + all subsequent RSP/DAT flits). |
| `Joint<config>` | A container that tracks all in-flight transactions for one node scope. Feed every flit into the joint; it constructs and updates `Xaction` objects automatically. |
| `RNFJoint<config>` | Joint for an **RN-F** (Full-function Request Node) perspective. Tracks outgoing REQ (`NextTXREQ`) and incoming SNP (`NextRXSNP`). |
| `SNFJoint<config>` | Joint for an **SN-F** (Full-function Subordinate Node) perspective. Tracks incoming REQ (`NextRXREQ`) and outgoing RSP/DAT. |
| `XactDenialEnum` | Return value of every `Next*` call. `XactDenial::ACCEPTED` means the flit was valid; any other value explains the violation. |
| `Global<config>` | Holds the node topology and flit-field checking configuration shared across joints. |
| `Companion` | A generic key-value bag attached to every `Xaction` for storing user-defined metadata. |

---

## 2. Setup: FlitConfiguration

Every template in CHIron is parameterised by a `FlitConfiguration`. Choose the issue-specific namespace, then instantiate `FlitConfiguration` with your bus parameters.

```cpp
// Issue E / Early-B
#include "chi_eb/xact/chi_eb_joint.hpp"

using namespace CHI::Eb;

using config = FlitConfiguration<
    7,       // CHI_NODEID_WIDTH  – node ID bit width
    48,      // CHI_REQ_ADDR_WIDTH – request address bit width
    0,       // CHI_REQ_RSVDC_WIDTH
    0,       // CHI_DAT_RSVDC_WIDTH
    256,     // CHI_DATA_WIDTH – data bus width in bits
    false,   // CHI_DATACHECK_PRESENT
    false,   // CHI_POISON_PRESENT
    false    // CHI_MPAM_PRESENT
>;
```

For Issue B:

```cpp
#include "chi_b/spec/chi_b_protocol.hpp"
#include "chi_b/xact/chi_b_joint.hpp"

using namespace CHI::B;

using config = FlitConfiguration<7, 48, 0, 0, 256, false, false>;
```

> All subsequent code examples use `CHI::Eb` and the `config` alias above.

---

## 3. Setup: Global and Topology

`Global<config>` carries shared configuration. The most important field is `TOPOLOGY`, which maps node IDs to their types.

```cpp
#include "chi_eb/xact/chi_eb_joint.hpp"
using namespace CHI::Eb;

Xact::Global<config> glbl;

// Register nodes: SetNode(id, type [, name])
glbl.TOPOLOGY.SetNode(0x01, Xact::NodeType::RN_F, "cpu0");
glbl.TOPOLOGY.SetNode(0x02, Xact::NodeType::RN_F, "cpu1");
glbl.TOPOLOGY.SetNode(0x10, Xact::NodeType::HN_F, "hnf0");
glbl.TOPOLOGY.SetNode(0x20, Xact::NodeType::SN_F, "snf0");

// Optional: disable flit field checking during bringup
glbl.CHECK_FIELD_MAPPING.enable = false;
```

Supported node types are: `RN_F`, `RN_D`, `RN_I`, `HN_F`, `HN_I`, `SN_F`, `SN_I`, `MN`.

---

## 4. Transaction Types

Every `Xaction` carries an `XactionType` that describes the logical role of the request:

| `XactionType` | Description |
|---|---|
| `AllocatingRead` | Read that allocates a cache line (ReadShared, ReadClean, ReadUnique, …) |
| `NonAllocatingRead` | Non-allocating read (ReadNoSnp, ReadOnce, …) |
| `ImmediateWrite` | Immediate write (WriteNoSnpPtl, WriteNoSnpFull, …) |
| `WriteZero` | Write-zero (WriteNoSnpZero, WriteUniqueZero, …) |
| `CopyBackWrite` | Copy-back write (WriteBackFull, WriteCleanFull, WriteEvictFull, …) |
| `Atomic` | Atomic operation (AtomicStore, AtomicLoad, AtomicSwap, AtomicCompare) |
| `IndependentStash` | Independent stash (StashOnceShared, StashOnceUnique, …) |
| `Dataless` | Dataless request (CleanUnique, MakeUnique, CleanShared, Evict, …) |
| `HomeRead` | Read seen from HN-F / SN-F perspective |
| `HomeWrite` | Write seen from HN-F / SN-F perspective |
| `HomeWriteZero` | Write-zero seen from HN-F / SN-F perspective |
| `HomeDataless` | Dataless seen from HN-F / SN-F perspective |
| `HomeAtomic` | Atomic seen from HN-F / SN-F perspective |
| `HomeSnoop` | Home-originated snoop |
| `ForwardSnoop` | Forward snoop (SnpSharedFwd, SnpUniqueFwd, …) |
| `DVMSnoop` | DVM snoop |

The type is chosen automatically by the joint based on the REQ/SNP opcode; you do not set it manually.

---

## 5. Joints – the entry point

A **Joint** is the main object you interact with. Instantiate one per observed node and keep it alive for the lifetime of the trace.

```cpp
// One joint per RN-F node
Xact::RNFJoint<config> rnJoint;

// One joint for the SN-F (home or subordinate perspective)
Xact::SNFJoint<config> snJoint;
```

### RNFJoint channels

| Method | Direction | Channel |
|---|---|---|
| `NextTXREQ(glbl, time, req, &xaction)` | TX | REQ sent by the RN |
| `NextRXSNP(glbl, time, snp, tgtId, &xaction)` | RX | SNP received by the RN |
| `NextTXRSP(glbl, time, rsp, &xaction)` | TX | RSP sent by the RN (CompAck, SnpResp, …) |
| `NextRXRSP(glbl, time, rsp, &xaction)` | RX | RSP received by the RN (Comp, DBIDResp, …) |
| `NextTXDAT(glbl, time, dat, &xaction)` | TX | DAT sent by the RN (CopyBackWrData, …) |
| `NextRXDAT(glbl, time, dat, &xaction)` | RX | DAT received by the RN (CompData, …) |

### SNFJoint channels

| Method | Direction | Channel |
|---|---|---|
| `NextRXREQ(glbl, time, req, &xaction)` | RX | REQ received by the SN |
| `NextTXRSP(glbl, time, rsp, &xaction)` | TX | RSP sent by the SN (CompDBIDResp, DBIDResp, …) |
| `NextTXDAT(glbl, time, dat, &xaction)` | TX | DAT sent by the SN (CompData, DataSepResp, …) |
| `NextRXDAT(glbl, time, dat, &xaction)` | RX | DAT received by the SN (WriteData, …) |

> The `&xaction` output parameter is optional. Pass `nullptr` if you do not need the associated `Xaction` pointer.

---

## 6. Feeding flits and reading denials

Every `Next*` call returns an `XactDenialEnum`. Always check it.

```cpp
std::shared_ptr<Xact::Xaction<config>> xaction;
Xact::XactDenialEnum denial;

// ── Feed a TXREQ flit ──────────────────────────────────────────────────────
Flits::REQ<config> req = /* ... deserialise from raw bits ... */;

denial = rnJoint.NextTXREQ(glbl, /*time=*/cycle, req, &xaction);

if (denial == Xact::XactDenial::ACCEPTED) {
    // xaction points to the newly created (or retried) Xaction
} else {
    std::cerr << "REQ denied: " << denial->name << "\n";
}

// ── Feed an RXRSP flit ────────────────────────────────────────────────────
Flits::RSP<config> rsp = /* ... */;
denial = rnJoint.NextRXRSP(glbl, cycle, rsp, &xaction);

// ── Feed an RXDAT flit ────────────────────────────────────────────────────
Flits::DAT<config> dat = /* ... */;
denial = rnJoint.NextRXDAT(glbl, cycle, dat, &xaction);
```

### Denial values

| Denial | Meaning |
|---|---|
| `XactDenial::ACCEPTED` | Flit is protocol-correct |
| `XactDenial::DENIED_UNSUPPORTED_FEATURE` | Opcode or feature not yet modelled |
| `XactDenial::INVALID_INITIAL` | Initial cache state is illegal for this request |
| `XactDenial::INVALID_SEQUENCE` | Flit arrived out of sequence for its transaction |
| `XactDenial::INVALID_FIELD` | A flit field value violates the specification |

---

## 7. Event callbacks

Joints fire events on every significant transition. Subscribe using the public `EventBus` members.

```cpp
// Called each time a new transaction is accepted (first REQ/SNP admitted)
rnJoint.OnAccepted.Register([](Xact::JointXactionAcceptedEvent<config>& ev) {
    auto xact = ev.GetXaction();
    std::cout << "New xact TxnID=" << (int)xact->GetFirst().flit.req.TxnID
              << " type=" << (int)xact->GetType() << "\n";
});

// Called when a transaction is marked complete
rnJoint.OnComplete.Register([](Xact::JointXactionCompleteEvent<config>& ev) {
    auto xact = ev.GetXaction();
    std::cout << "Complete xact TxnID=" << (int)xact->GetFirst().flit.req.TxnID << "\n";
});

// Called when a transaction is retried (got RetryAck + PCrdGrant)
rnJoint.OnRetry.Register([](Xact::JointXactionRetryEvent<config>& ev) {
    std::cout << "Retrying xact\n";
});

// Called when a REQ/SNP flit is denied
rnJoint.OnDeniedRequest.Register([](Xact::JointDeniedRequestEvent<config>& ev) {
    std::cerr << "Denied REQ: " << ev.GetDenial()->name << "\n";
});

// Called when an RSP/DAT flit is denied
rnJoint.OnDeniedResponse.Register([](Xact::JointDeniedResponseEvent<config>& ev) {
    std::cerr << "Denied RSP/DAT: " << ev.GetDenial()->name << "\n";
});
```

`EventBus::Register` returns a token; store it as long as you want the callback to be active. Destroy the token to unsubscribe. Alternatively, call `ClearListeners()` on the joint to remove all callbacks at once.

---

## 8. Inspecting a completed Xaction

```cpp
void InspectXaction(const Xact::Xaction<config>& xact,
                    const Xact::Global<config>&  glbl)
{
    // ── First (request) flit ──────────────────────────────────────────────
    const Xact::FiredRequestFlit<config>& first = xact.GetFirst();
    std::cout << "TxnID : " << (int)first.flit.req.TxnID  << "\n";
    std::cout << "Addr  : 0x" << std::hex << first.flit.req.Addr << std::dec << "\n";
    std::cout << "Time  : " << first.time << "\n";

    // ── Transaction type ──────────────────────────────────────────────────
    std::cout << "Type  : " << (int)xact.GetType() << "\n";

    // ── Completeness ─────────────────────────────────────────────────────
    std::cout << "Done  : " << xact.IsComplete(glbl) << "\n";

    // ── Denial on first flit ──────────────────────────────────────────────
    if (xact.GetFirstDenial() != Xact::XactDenial::ACCEPTED)
        std::cout << "First denial: " << xact.GetFirstDenial()->name << "\n";

    // ── Walk response/data flits ──────────────────────────────────────────
    const auto& subseq = xact.GetSubsequence();
    for (size_t i = 0; i < subseq.size(); ++i) {
        const Xact::FiredResponseFlit<config>& rsp = subseq[i];
        if (rsp.isRSP)
            std::cout << "  RSP[" << i << "] opcode=" << (int)rsp.flit.rsp.Opcode << "\n";
        else
            std::cout << "  DAT[" << i << "] opcode=" << (int)rsp.flit.dat.Opcode << "\n";
    }

    // ── Check for specific opcodes in the response sequence ───────────────
    bool hasCompData = xact.HasDAT({ Opcodes::DAT::CompData });
    bool hasComp     = xact.HasRSP({ Opcodes::RSP::Comp      });
    std::cout << "HasCompData=" << hasCompData << " HasComp=" << hasComp << "\n";
}
```

### Querying in-flight transactions

```cpp
std::vector<std::shared_ptr<Xact::Xaction<config>>> inflight;
rnJoint.GetInflight(glbl, inflight, /*sortByTime=*/true);

for (auto& xact : inflight) {
    std::cout << "In-flight TxnID="
              << (int)xact->GetFirst().flit.req.TxnID << "\n";
}
```

---

## 9. Retry and resend handling

CHI allows a requester to retry a request after receiving `RetryAck` + `PCrdGrant`. The joint handles this automatically:

- When the second attempt is admitted, `xact->IsSecondTry()` returns `true`.
- `xact->GetFirstTry()` returns a `shared_ptr` to the original `Xaction`.

```cpp
if (xact->IsSecondTry()) {
    auto firstTry = xact->GetFirstTry();
    std::cout << "Retry of TxnID=" << (int)firstTry->GetFirst().flit.req.TxnID << "\n";
}
```

---

## 10. Using the Companion to attach custom data

Every `Xaction` and `FiredFlit` carries a `Companion` object – a lightweight key-value store for user-defined metadata.

```cpp
// Define a key (use a static/global so the pointer is stable)
static const Xact::CompanionKey<uint64_t> MY_ORDINAL;

// Store a value
xaction->companion.Set(&MY_ORDINAL, ordinal);

// Retrieve a value (returns std::optional<T>)
auto val = xaction->companion.Get(&MY_ORDINAL);
if (val) std::cout << "ordinal=" << *val << "\n";
```

---

## 11. Complete worked example

The following self-contained example replays a minimal ReadClean transaction from an RN-F's perspective.

```cpp
#define CHI_ISSUE_EB_ENABLE
#include "chi_eb/xact/chi_eb_joint.hpp"

#include <iostream>
#include <cassert>

using namespace CHI::Eb;

// ── Configuration ────────────────────────────────────────────────────────────
using config = FlitConfiguration<7, 48, 0, 0, 256, false, false, false>;

int main()
{
    // ── Global + Topology ────────────────────────────────────────────────────
    Xact::Global<config> glbl;
    glbl.TOPOLOGY.SetNode(0x01, Xact::NodeType::RN_F, "cpu0");
    glbl.TOPOLOGY.SetNode(0x10, Xact::NodeType::HN_F, "hnf0");
    glbl.CHECK_FIELD_MAPPING.enable = false;   // relax field checks

    // ── Joint for cpu0 ───────────────────────────────────────────────────────
    Xact::RNFJoint<config> joint;

    joint.OnAccepted.Register([](Xact::JointXactionAcceptedEvent<config>& ev) {
        std::cout << "[OnAccepted] TxnID=" << (int)ev.GetXaction()->GetFirst().flit.req.TxnID << "\n";
    });
    joint.OnComplete.Register([](Xact::JointXactionCompleteEvent<config>& ev) {
        std::cout << "[OnComplete] TxnID=" << (int)ev.GetXaction()->GetFirst().flit.req.TxnID << "\n";
    });

    std::shared_ptr<Xact::Xaction<config>> xaction;
    Xact::XactDenialEnum denial;

    // ── Cycle 100: TXREQ – ReadClean ─────────────────────────────────────────
    Flits::REQ<config> req{};
    req.Opcode  = Opcodes::REQ::ReadClean;
    req.SrcID   = 0x01;
    req.TxnID   = 5;
    req.Addr    = 0x0000'1000;
    req.Size    = 6;   // 64 bytes

    denial = joint.NextTXREQ(glbl, /*time=*/100, req, &xaction);
    assert(denial == Xact::XactDenial::ACCEPTED);
    assert(xaction != nullptr);

    // ── Cycle 105: RXDAT – CompData(UC) ──────────────────────────────────────
    Flits::DAT<config> dat{};
    dat.Opcode  = Opcodes::DAT::CompData;
    dat.TxnID   = 5;
    dat.SrcID   = 0x10;
    dat.TgtID   = 0x01;
    dat.Resp    = Resps::CompData_UC;
    dat.DBID    = 0;

    denial = joint.NextRXDAT(glbl, /*time=*/105, dat, &xaction);
    assert(denial == Xact::XactDenial::ACCEPTED);

    // ── Cycle 106: TXRSP – CompAck ───────────────────────────────────────────
    Flits::RSP<config> rsp{};
    rsp.Opcode  = Opcodes::RSP::CompAck;
    rsp.TxnID   = 0;   // DBID from CompData
    rsp.SrcID   = 0x01;
    rsp.TgtID   = 0x10;

    denial = joint.NextTXRSP(glbl, /*time=*/106, rsp, &xaction);
    assert(denial == Xact::XactDenial::ACCEPTED);
    assert(xaction->IsComplete(glbl));

    std::cout << "Transaction complete. Final state: "
              << Xact::CacheStates::UC.ToString() << "\n";
    return 0;
}
```

Expected output:

```
[OnAccepted] TxnID=5
[OnComplete] TxnID=5
Transaction complete. Final state: UC
```

---

## 12. Common pitfalls

| Problem | Cause | Fix |
|---|---|---|
| `NextTXREQ` returns `DENIED_UNSUPPORTED_FEATURE` | Opcode is not modelled (DVM, MTE, …) | Expected for unsupported opcodes; suppress if known. |
| Transaction never fires `OnComplete` | A required flit (e.g. CompAck) was not fed | Feed every flit on every channel, including TX RSP/DAT. |
| `xaction` pointer is null after `NextRXRSP` | RSP was matched to an existing xaction created earlier | The pointer is set only when the flit opens **or** closes a transaction; for mid-sequence flits it may be null unless `&xaction` already held the pointer. |
| Flit rejected with `INVALID_INITIAL` | Initial cache state in `RNCacheStateMap` is wrong | See the [State Trackers guide](quick-guide-state-trackers.md) to keep the state map in sync. |
| Events fire unexpectedly on `Clear()` | `Clear()` does not remove listeners | Call `ClearListeners()` before `Clear()` if you want to suppress events during teardown. |
