# Transaction Models: RNFJoint and SNFJoint

## Introduction

CHIron's *transaction model* components track the full lifecycle of CHI transactions —
from the initial request flit through every response and data beat — and validate that
the observed flit sequence conforms to the AMBA CHI specification.

Two concrete classes are provided:

| Class | Node perspective | Channels observed |
|-------|-----------------|-------------------|
| `RNFJoint<config>` | Requester Node (RN-F) | TXREQ, RXSNP, TXRSP, RXRSP, TXDAT, RXDAT |
| `SNFJoint<config>` | Home / Subordinate Node (HN-F / SN-F) | RXREQ, TXSNP, TXRSP, RXRSP, TXDAT, RXDAT |

Both inherit from `Joint<config>`, which exposes an event-bus system for hooking into
key transaction lifecycle events.

---

## Configuration

All template classes are parameterised on a **flit-configuration** type that pins the
width of every protocol field at compile time:

```cpp
#include "chi_eb/xact/chi_eb_joint.hpp"

using namespace CHI::Eb;

using config = FlitConfiguration<
    7,      // NodeIDWidth    (7–16 bits)
    48,     // ReqAddrWidth   (up to 52 bits)
    0,      // ReqRSVDCWidth  (0–8 bits)
    0,      // DatRSVDCWidth  (0–8 bits)
    256,    // DataWidth      (64/128/256/512 bits)
    false,  // DataCheckPresent
    false,  // PoisonPresent
    false   // MPAMPresent    (CHI Issue E.b only)
>;
```

---

## Global Context

Every flit-feed call requires a `Global<config>` object that describes the network
topology and enables optional field-mapping checks:

```cpp
Xact::Global<config> glbl;

// Register the nodes that are present in your design
glbl.TOPOLOGY.SetNode(0x04, NodeType::RN_F, "cpu0");
glbl.TOPOLOGY.SetNode(0x08, NodeType::RN_F, "cpu1");
glbl.TOPOLOGY.SetNode(0x40, NodeType::HN_F, "hnf0");
glbl.TOPOLOGY.SetNode(0x80, NodeType::SN_F, "snf0");

// Optionally disable strict field-mapping checks (useful during bring-up)
glbl.CHECK_FIELD_MAPPING.enable = false;
```

### Node Types

| `NodeType::*` | Meaning |
|---------------|---------|
| `RN_F` | Fully-coherent Requester Node |
| `RN_D` | Dvm-capable Requester Node |
| `RN_I` | Non-coherent Requester Node |
| `HN_F` | Fully-coherent Home Node |
| `HN_I` | Non-coherent Home Node |
| `SN_F` | Fully-coherent Subordinate Node |
| `SN_I` | Non-coherent Subordinate Node |
| `MN` | Miscellaneous Node |

---

## RNFJoint — Requester Node Transaction Model

### Instantiation

```cpp
Xact::RNFJoint<config> rnJoint;
```

### Feeding Flits

Call the matching `Next*` method each time a flit is observed on the corresponding
channel. Each call returns an `XactDenialEnum` (an int-convertible pointer):

```cpp
Flits::REQ<config> reqFlit;
// … populate reqFlit fields …

std::shared_ptr<Xaction<config>> xaction;   // optional out-parameter

Xact::XactDenialEnum denial =
    rnJoint.NextTXREQ(glbl, cycleTime, reqFlit, &xaction);

if (denial != Xact::XactDenial::ACCEPTED) {
    std::cerr << "Protocol violation: " << denial->name << "\n";
}
```

| Method | Channel | Flit type |
|--------|---------|-----------|
| `NextTXREQ(glbl, time, reqFlit, &xact)` | TXREQ | `Flits::REQ<config>` |
| `NextRXSNP(glbl, time, snpFlit, &xact)` | RXSNP | `Flits::SNP<config>` |
| `NextTXRSP(glbl, time, rspFlit, &xact)` | TXRSP | `Flits::RSP<config>` |
| `NextRXRSP(glbl, time, rspFlit, &xact)` | RXRSP | `Flits::RSP<config>` |
| `NextTXDAT(glbl, time, datFlit, &xact)` | TXDAT | `Flits::DAT<config>` |
| `NextRXDAT(glbl, time, datFlit, &xact)` | RXDAT | `Flits::DAT<config>` |

The `xaction` out-parameter is set to the `Xaction` object being updated.  
Pass `nullptr` if you do not need the xaction reference.

### Querying In-Flight Transactions

```cpp
std::vector<std::shared_ptr<Xaction<config>>> inflight;
rnJoint.GetInflight(glbl, inflight, /*sortByTime=*/true);

for (auto& xact : inflight) {
    std::cout << "In-flight xact type: " << (int)xact->GetType() << "\n";
}
```

### Subscribing to Events

`Joint<config>` inherits from several `Gravity::EventBus<>` specialisations.
Register a listener to react to transaction lifecycle events:

```cpp
rnJoint.OnComplete += [](const Xact::JointXactionCompleteEvent<config>& ev) {
    auto& xact = *ev.xaction;
    std::cout << "Transaction complete, type=" << (int)xact.GetType() << "\n";
};

rnJoint.OnDeniedRequest += [](const Xact::JointDeniedRequestEvent<config>& ev) {
    std::cerr << "Denied request: " << ev.denial->name << "\n";
};
```

Available event buses:

| Event bus | Fires when |
|-----------|-----------|
| `OnDeniedRequest` | A request flit was rejected |
| `OnDeniedResponse` | A response/data flit was rejected |
| `OnAccepted` | A new transaction was accepted |
| `OnRetry` | A transaction was retried |
| `OnTxnIDAllocation` | A TxnID was allocated |
| `OnTxnIDFree` | A TxnID was freed |
| `OnDBIDAllocation` | A DBID was allocated |
| `OnDBIDFree` | A DBID was freed |
| `OnComplete` | A transaction reached completion |

---

## SNFJoint — Home / Subordinate Node Transaction Model

`SNFJoint<config>` tracks transactions from the perspective of the responding node.

### Instantiation

```cpp
Xact::SNFJoint<config> snJoint;
```

### Feeding Flits

| Method | Channel | Flit type |
|--------|---------|-----------|
| `NextRXREQ(glbl, time, reqFlit, &xact)` | RXREQ | `Flits::REQ<config>` |
| `NextTXRSP(glbl, time, rspFlit, &xact)` | TXRSP | `Flits::RSP<config>` |
| `NextTXDAT(glbl, time, datFlit, &xact)` | TXDAT | `Flits::DAT<config>` |
| `NextRXDAT(glbl, time, datFlit, &xact)` | RXDAT | `Flits::DAT<config>` |

Usage is identical to `RNFJoint` — populate a flit struct, call the method, check the
returned denial code.

---

## Denial Codes

The return type of all `Next*` methods is `Xact::XactDenialEnum`, which is a pointer to
a `XactDenialEnumBack` object.  It is directly comparable, and its `value` field is an
int (0 = accepted).

```cpp
// Accepted check
if (denial == Xact::XactDenial::ACCEPTED) { /* OK */ }

// Integer value (0 = accepted, non-zero = violation)
int code = denial->value;

// Human-readable name
const char* msg = denial->name;   // e.g. "XACT_DENIED_CHANNEL_TXREQ"
```

Common denial codes:

| Code | Meaning |
|------|---------|
| `XACT_ACCEPTED` (0) | Flit accepted |
| `XACT_DENIED_CHANNEL_TXREQ` | Wrong flit on TXREQ channel |
| `XACT_DENIED_TXNID_IN_USE` | TxnID already in use |
| `XACT_DENIED_TXNID_NOT_EXIST` | Response for unknown TxnID |
| `XACT_DENIED_DBID_IN_USE` | DBID already allocated |
| `XACT_DENIED_NESTING_SNP` | Illegal snoop nesting |
| `XACT_DENIED_REQ_OPCODE` | Unrecognised REQ opcode |

---

## Transaction Types

`Xaction<config>::GetType()` returns an `XactionType` enum value:

| Type | Description |
|------|-------------|
| `AllocatingRead` | Read with cache-line allocation |
| `NonAllocatingRead` | Read without allocation |
| `ImmediateWrite` | Non-copy-back write |
| `WriteZero` | Write-zeroed line |
| `CopyBackWrite` | Write-back from cache |
| `Atomic` | Atomic read-modify-write |
| `IndependentStash` | Stash without data return |
| `Dataless` | CMO / MakeInvalid / Evict etc. |
| `HomeRead` | Home-node read response flow |
| `HomeWrite` | Home-node write response flow |
| `HomeAtomic` | Home-node atomic response flow |
| `HomeSnoop` | Home-initiated snoop |
| `ForwardSnoop` | Forwarded snoop |

---

## Complete Example

```cpp
#include "chi_eb/xact/chi_eb_joint.hpp"

using namespace CHI::Eb;
using config = FlitConfiguration<7, 48, 0, 0, 256>;

int main() {
    // 1. Build global context
    Xact::Global<config> glbl;
    glbl.TOPOLOGY.SetNode(0x04, NodeType::RN_F, "cpu0");
    glbl.TOPOLOGY.SetNode(0x40, NodeType::HN_F, "hnf0");

    // 2. Create joint
    Xact::RNFJoint<config> rnJoint;

    // 3. Subscribe to completion events
    rnJoint.OnComplete += [](const Xact::JointXactionCompleteEvent<config>& ev) {
        std::cout << "[CHI] Xaction complete, type=" << (int)ev.xaction->GetType() << "\n";
    };

    // 4. Simulate: feed a ReadUnique request
    Flits::REQ<config> req{};
    req.QoS()     = 0;
    req.TgtID()   = 0x40;
    req.SrcID()   = 0x04;
    req.TxnID()   = 1;
    req.Opcode()  = Opcodes::REQ::ReadUnique;
    req.Size()    = 6;       // 64 bytes
    req.Addr()    = 0x1000;
    req.AllowRetry() = 1;

    std::shared_ptr<Xaction<config>> xact;
    Xact::XactDenialEnum d = rnJoint.NextTXREQ(glbl, /*time=*/100, req, &xact);
    if (d != Xact::XactDenial::ACCEPTED)
        std::cerr << "Error: " << d->name << "\n";

    // 5. Feed the CompData response
    Flits::DAT<config> dat{};
    dat.TgtID()  = 0x04;
    dat.SrcID()  = 0x40;
    dat.TxnID()  = 1;
    dat.Opcode() = Opcodes::DAT::CompData;
    dat.Resp()   = Resps::CompData_UC;
    // ... data payload ...

    rnJoint.NextRXDAT(glbl, /*time=*/200, dat, nullptr);

    return 0;
}
```
