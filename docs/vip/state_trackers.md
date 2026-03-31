# Cache State Tracker: RNCacheStateMap

## Introduction

`RNCacheStateMap<config>` maintains a per-address map of CHI cache-coherence states
for a single Requester Node (RN-F).  It validates that every outgoing request and every
incoming snoop is consistent with the current coherence state, and it updates the stored
state as transactions complete.

The class is designed to be used *alongside* `RNFJoint`: the Joint tracks the transaction
lifecycle while the state map tracks what happens to individual cache lines.

---

## Cache States

The CHI coherence protocol defines seven states, represented by the `CacheState` type:

| State | `CacheStates::*` | Meaning |
|-------|------------------|---------|
| **UC** | `CacheStates::UC` | Unique Clean — sole copy, clean |
| **UCE** | `CacheStates::UCE` | Unique Clean Exclusive — same as UC, exclusive hint |
| **UD** | `CacheStates::UD` | Unique Dirty — sole copy, dirty |
| **UDP** | `CacheStates::UDP` | Unique Dirty Private — UD variant |
| **SC** | `CacheStates::SC` | Shared Clean — shared copy, clean |
| **SD** | `CacheStates::SD` | Shared Dirty — shared copy, dirty |
| **I** | `CacheStates::I` | Invalid — not cached |
| *(none)* | `CacheStates::None` | No/unknown state (result of invalid transition) |

`CacheState` is a bitmask: multiple states can be OR-ed to express a *set* of permitted
outcomes.  For example, `CacheStates::UC | CacheStates::SC` means "either UC or SC is
acceptable".

The raw storage is `uint8_t i8` with one bit per state:

```
Bit 0: UC | Bit 1: UCE | Bit 2: UD | Bit 3: UDP | Bit 4: SC | Bit 5: SD | Bit 6: I
```

---

## Silent Transitions

The state map supports *silent* state changes — transitions that happen without an
explicit protocol message (allowed by the CHI specification):

| Method | Controls |
|--------|---------|
| `SetSilentEviction(bool)` | Cache line quietly evicted (state → I) |
| `SetSilentSharing(bool)` | Line silently downgraded to SC |
| `SetSilentStore(bool)` | Clean line silently written (SC/UC → UD) |
| `SetSilentInvalidation(bool)` | Line silently invalidated (any → I) |

All are **disabled by default**.  Enable only those that your DUT is known to perform.

```cpp
Xact::RNCacheStateMap<config> rnStates;
rnStates.SetSilentEviction(true);
rnStates.SetSilentInvalidation(true);
```

---

## Initialising a Cache-Line State

```cpp
// Set the state for a 64-byte aligned address
uint64_t addr = 0x1000;
rnStates.Set(addr, Xact::CacheStates::UC);

// Query the current state
Xact::CacheState current = rnStates.Get(addr);
```

---

## Validating Requests and Snoops

### Outgoing Request (TXREQ)

```cpp
Flits::REQ<config> req;
// ... populate req ...

Xact::XactDenialEnum d = rnStates.NextTXREQ(req.Addr(), req);
if (d != Xact::XactDenial::ACCEPTED)
    reportError("State violation on TXREQ: ", d->name);
```

`NextTXREQ` checks that the initial cache state is consistent with the request opcode
(e.g., a `CleanUnique` request from state I is invalid) and records the pending
transition.

### Incoming Snoop (RXSNP)

```cpp
Flits::SNP<config> snp;
// ... populate snp ...

// Note: SNP.Addr is snpAddrWidth = reqAddrWidth - 3
uint64_t addr = uint64_t(snp.Addr()) << 3;

Xact::XactDenialEnum d = rnStates.NextRXSNP(addr, snp);
```

`NextRXSNP` checks that a snoop can legally arrive given the current state and
snoop opcode.

### Outgoing Snoop Response (TXRSP / TXDAT, with xaction)

When a snoop response includes a state change, the state map needs the in-flight
xaction to validate the final state.

```cpp
// After calling rnJoint.NextTXRSP(..., &xaction) successfully:
Xact::XactDenialEnum d = rnStates.NextTXRSP(addr, *xaction, rspFlit);
```

---

## Applying a State Transition (Transfer)

`Transfer` finalises the coherence-state update after a transaction completes.

```cpp
// Without xaction validation — directly sets the new state
Xact::XactDenialEnum d =
    rnStates.Transfer(addr, Xact::CacheStates::UC);

// With xaction validation — verifies that the transition is legal given
// the completed transaction type and its response
d = rnStates.Transfer(addr, Xact::CacheStates::UC, xaction.get());
```

When `nestingXaction == nullptr` the call is equivalent to `Set(addr, newState)` and
always returns `XACT_ACCEPTED`.

---

## Seer Mode (Speculative State Inference)

The state map can optionally track an access pattern and speculatively infer states
for addresses not yet explicitly set.  This is activated automatically for ranges
configured during construction, and is queried via:

```cpp
Xact::CacheState inferred = rnStates.EvaluateSilently(currentState);
auto [state, confident] = rnStates.EvaluateWithSeer(addr);
```

This is an advanced feature; for most VIP use cases `Set`/`Get`/`Transfer` are sufficient.

---

## Typical Integration with RNFJoint

The most common pattern is to call both the Joint and the state map for every flit,
then apply the final state in the Joint's `OnComplete` handler:

```cpp
Xact::RNFJoint<config>      rnJoint;
Xact::RNCacheStateMap<config> rnStates;

// On TXREQ: validate against joint (protocol) AND state map (coherence)
auto process_txreq = [&](const Flits::REQ<config>& req, uint64_t time) {
    std::shared_ptr<Xaction<config>> xact;
    Xact::XactDenialEnum d1 = rnJoint.NextTXREQ(glbl, time, req, &xact);
    Xact::XactDenialEnum d2 = rnStates.NextTXREQ(req.Addr(), req);
    if (d1 != Xact::XactDenial::ACCEPTED) reportViolation("Joint", d1);
    if (d2 != Xact::XactDenial::ACCEPTED) reportViolation("StateMap", d2);
};

// On RXSNP: validate against joint AND state map
auto process_rxsnp = [&](const Flits::SNP<config>& snp, uint64_t time) {
    std::shared_ptr<Xaction<config>> xact;
    uint64_t addr = uint64_t(snp.Addr()) << 3;
    rnJoint.NextRXSNP(glbl, time, snp, &xact);
    rnStates.NextRXSNP(addr, snp);
};

// On transaction complete: advance the cached state
rnJoint.OnComplete += [&](const Xact::JointXactionCompleteEvent<config>& ev) {
    const auto& xact = *ev.xaction;
    // Determine the final state from the last RSP/DAT response code,
    // then Transfer to record it.
    uint64_t addr = xact.GetFirst().flit.req.Addr();
    Xact::CacheState finalState = /* decode from xact.GetLastRSP() or GetLastDAT() */;
    rnStates.Transfer(addr, finalState, &xact);
};
```

---

## Complete Standalone Example

```cpp
#include "chi_eb/xact/chi_eb_xact_state.hpp"
#include "chi_eb/xact/chi_eb_joint.hpp"

using namespace CHI::Eb;
using config = FlitConfiguration<7, 48, 0, 0, 256>;

int main() {
    Xact::Global<config> glbl;
    glbl.TOPOLOGY.SetNode(0x04, NodeType::RN_F, "cpu0");
    glbl.TOPOLOGY.SetNode(0x40, NodeType::HN_F, "hnf0");
    glbl.CHECK_FIELD_MAPPING.enable = false;

    Xact::RNFJoint<config>        rnJoint;
    Xact::RNCacheStateMap<config> rnStates;

    // Pre-load cache state: address 0x1000 is held UC
    rnStates.Set(0x1000, Xact::CacheStates::UC);

    // Simulate a CleanUnique request from UC (expected: accepted)
    Flits::REQ<config> req{};
    req.TgtID()   = 0x40;
    req.SrcID()   = 0x04;
    req.TxnID()   = 2;
    req.Opcode()  = Opcodes::REQ::CleanUnique;
    req.Addr()    = 0x1000;

    std::shared_ptr<Xaction<config>> xact;
    rnJoint.NextTXREQ(glbl, 100, req, &xact);

    Xact::XactDenialEnum d = rnStates.NextTXREQ(0x1000, req);
    assert(d == Xact::XactDenial::ACCEPTED);  // UC → CleanUnique is valid

    // After Comp response, the line moves to UC
    rnStates.Transfer(0x1000, Xact::CacheStates::UC, xact.get());
    assert(rnStates.Get(0x1000) == Xact::CacheStates::UC);

    return 0;
}
```
