# Guide: Adding New Transaction Models

This guide explains how to implement a new transaction model (Xaction) end-to-end across all relevant components of CHIron.

---

## Overview

A *transaction* in CHIron is a high-level abstraction that groups:

1. A single *first flit* — the REQ or SNP flit that initiates the transaction.
2. A *subsequence* of RSP and DAT flits that respond to the initiating flit.

Each transaction type is modelled by a dedicated class that inherits from `Xaction<config>`.  The class encodes which REQ/SNP opcodes it accepts, how it absorbs subsequent flits, and when it is considered complete.

The full class hierarchy is:

```
Xaction<config>   (chi/xact/chi_xactions/chi_xactions_base.hpp)
├── XactionAllocatingRead<config>
├── XactionNonAllocatingRead<config>
├── XactionImmediateWrite<config>
├── XactionWriteZero<config>
├── XactionCopyBackWrite<config>
├── XactionAtomic<config>
├── XactionIndependentStash<config>
├── XactionDataless<config>
├── XactionHomeRead<config>
├── XactionHomeWrite<config>
├── XactionHomeWriteZero<config>
├── XactionHomeDataless<config>
├── XactionHomeAtomic<config>
├── XactionHomeSnoop<config>
├── XactionForwardSnoop<config>
└── XactionDVMSnoop<config>
```

---

## Step 1 — Add an Entry to `XactionType`

**File:** `chi/xact/chi_xactions/chi_xactions_base.hpp`

`XactionType` is an `enum class` that uniquely identifies each transaction model.  Add your new type at the end:

```cpp
enum class XactionType {
    AllocatingRead = 0,
    NonAllocatingRead,
    // … existing entries …
    DVMSnoop,
    MyNewTransaction   // ← add here
};
```

---

## Step 2 — Create the Implementation File

**Location:** `chi/xact/chi_xactions/chi_xactions_impl_MyNewTransaction.hpp`

Use the issue-version guard idiom that every other implementation file uses.  The minimal skeleton is shown below; expand it as described in the following steps.

```cpp
//#pragma once

#include "includes.hpp"
#include "chi_xactions_base.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_XACTIONS_B__IMPL_MYNEWTRANSACTION)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_XACTIONS_EB__IMPL_MYNEWTRANSACTION))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_B__IMPL_MYNEWTRANSACTION
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_XACTIONS_EB__IMPL_MYNEWTRANSACTION
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept config>
        class XactionMyNewTransaction : public Xaction<config> {
        public:
            XactionMyNewTransaction(
                const Global<config>&               glbl,
                const FiredRequestFlit<config>&     first,
                std::shared_ptr<Xaction<config>>    retried = nullptr) noexcept;

        public:
            virtual std::shared_ptr<Xaction<config>>              Clone()          const noexcept override;
            std::shared_ptr<XactionMyNewTransaction<config>>      CloneAsIs()      const noexcept;

        public:
            virtual bool IsTxnIDComplete(const Global<config>& glbl) const noexcept override;
            virtual bool IsDBIDComplete (const Global<config>& glbl) const noexcept override;
            virtual bool IsComplete     (const Global<config>& glbl) const noexcept override;

        protected:
            virtual XactDenialEnum NextRSPNoRecord(
                const Global<config>&               glbl,
                const FiredResponseFlit<config>&    rspFlit,
                bool& hasDBID, bool& firstDBID) noexcept override;

            virtual XactDenialEnum NextDATNoRecord(
                const Global<config>&               glbl,
                const FiredResponseFlit<config>&    datFlit,
                bool& hasDBID, bool& firstDBID) noexcept override;
        };

    } // namespace Xact
/*
}
*/


// ─── Implementation ───────────────────────────────────────────────────────────
namespace /*CHI::*/Xact {

    // … method bodies go here (see Steps 3–7) …

} // namespace Xact


#endif // guard
```

---

## Step 3 — Implement the Constructor

The constructor is responsible for:

1. Calling the base constructor with the correct `XactionType`.
2. Validating the first flit (channel, opcode, direction).
3. Optionally running the field-mapping checker.

```cpp
template<FlitConfigurationConcept config>
inline XactionMyNewTransaction<config>::XactionMyNewTransaction(
    const Global<config>&               glbl,
    const FiredRequestFlit<config>&     firstFlit,
    std::shared_ptr<Xaction<config>>    retried) noexcept
    : Xaction<config>(XactionType::MyNewTransaction, firstFlit, retried)
{
    this->firstDenial = XactDenial::ACCEPTED;

    // 1. Check that the first flit arrived on the expected channel.
    if (!this->first.IsREQ())   // or IsSNP() for snoop-initiated transactions
    {
        this->firstDenial = XactDenial::DENIED_CHANNEL_NOT_REQ;
        return;
    }

    // 2. Accept only the REQ opcodes that belong to this transaction type.
    if (
        this->first.flit.req.Opcode() != Opcodes::REQ::MyPrimaryOpcode
#ifdef CHI_ISSUE_EB_ENABLE
     && this->first.flit.req.Opcode() != Opcodes::REQ::MyEBOpcode
#endif
    ) [[unlikely]]
    {
        this->firstDenial = XactDenial::DENIED_REQ_OPCODE;
        return;
    }

    // 3. Check the transaction flows from the correct node type to the correct node type.
    if (!this->first.IsFromRequesterToHome(glbl))
    {
        this->firstDenial = XactDenial::DENIED_REQ_NOT_FROM_RN_TO_HN;
        return;
    }

    // 4. Run the field-mapping checker (validates per-opcode field constraints).
    if (glbl.CHECK_FIELD_MAPPING.enable)
    {
        this->firstDenial = glbl.CHECK_FIELD_MAPPING.Check(firstFlit.flit.req);
        if (this->firstDenial != XactDenial::ACCEPTED)
            return;
    }
}
```

Available direction-checking helpers on `FiredRequestFlit`:

| Method | Meaning |
|--------|---------|
| `IsFromRequesterToHome(glbl)` | REQ from RN to HN (most read/write/dataless) |
| `IsFromHomeToRequester(glbl)` | SNP from HN to RN |
| `IsFromRequesterToSubordinate(glbl)` | REQ from RN to SN |
| `IsFromHomeToSubordinate(glbl)` | SNP/REQ from HN to SN |

---

## Step 4 — Implement `Clone()`

`Clone()` is required by the base class.  It simply copies the object:

```cpp
template<FlitConfigurationConcept config>
inline std::shared_ptr<Xaction<config>>
XactionMyNewTransaction<config>::Clone() const noexcept
{
    return std::static_pointer_cast<Xaction<config>>(CloneAsIs());
}

template<FlitConfigurationConcept config>
inline std::shared_ptr<XactionMyNewTransaction<config>>
XactionMyNewTransaction<config>::CloneAsIs() const noexcept
{
    return std::make_shared<XactionMyNewTransaction<config>>(*this);
}
```

---

## Step 5 — Implement Completion Predicates

Three pure-virtual predicates must be implemented.  They are called externally to determine the status of a transaction.

```cpp
// Returns true once the TxnID slot can be freed (Home perspective).
template<FlitConfigurationConcept config>
inline bool XactionMyNewTransaction<config>::IsTxnIDComplete(
    const Global<config>& glbl) const noexcept
{
    return IsComplete(glbl);
}

// Returns true once the DBID slot can be freed.
template<FlitConfigurationConcept config>
inline bool XactionMyNewTransaction<config>::IsDBIDComplete(
    const Global<config>& glbl) const noexcept
{
    return IsComplete(glbl);
}

// Returns true when the full transaction has concluded.
template<FlitConfigurationConcept config>
inline bool XactionMyNewTransaction<config>::IsComplete(
    const Global<config>& glbl) const noexcept
{
    // Example: complete when a Comp RSP has been received
    return this->HasRSP({ Opcodes::RSP::Comp });
}
```

Use `this->HasRSP(…)` and `this->HasDAT(…)` to query the accumulated subsequence.  For transactions that separate TxnID release from DBID release (e.g., writes), the three predicates return different values.

---

## Step 6 — Implement `NextRSPNoRecord` and `NextDATNoRecord`

These methods are called by external scoreboard infrastructure each time a RSP or DAT flit arrives that matches the transaction's TxnID.  They must either accept the flit (return `XactDenial::ACCEPTED`) or reject it with a specific denial reason.

The base class records the flit into `this->subsequence` **only if** you return `ACCEPTED`; the prefix `NoRecord` in the method name refers to the fact that you receive the flit *before* it has been appended.

```cpp
template<FlitConfigurationConcept config>
inline XactDenialEnum XactionMyNewTransaction<config>::NextRSPNoRecord(
    const Global<config>&               glbl,
    const FiredResponseFlit<config>&    rspFlit,
    bool& hasDBID, bool& firstDBID) noexcept
{
    // Guard: no more flits once the transaction is logically complete.
    if (IsComplete(glbl))
        return XactDenial::DENIED_COMPLETED_RSP;

    if (!rspFlit.IsRSP())
        return XactDenial::DENIED_CHANNEL_NOT_RSP;

    // Handle RetryAck / PCrdGrant (common pattern shared by all non-snoop transactions).
    if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
    {
        // AllowRetry checking is left to external scoreboards.
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
        if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
            return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;
        hasDBID = rspFlit.flit.rsp.DBID() != 0;
        return XactDenial::ACCEPTED;
    }

    return XactDenial::DENIED_RSP_OPCODE;
}


template<FlitConfigurationConcept config>
inline XactDenialEnum XactionMyNewTransaction<config>::NextDATNoRecord(
    const Global<config>&               glbl,
    const FiredResponseFlit<config>&    datFlit,
    bool& hasDBID, bool& firstDBID) noexcept
{
    if (IsComplete(glbl))
        return XactDenial::DENIED_COMPLETED_DAT;

    if (!datFlit.IsDAT())
        return XactDenial::DENIED_CHANNEL_NOT_DAT;

    // If this transaction type does not carry data, reject all DAT flits.
    return XactDenial::DENIED_DAT_OPCODE;
}
```

**`hasDBID` / `firstDBID` output parameters**

| Variable | Set to `true` when … |
|----------|----------------------|
| `hasDBID` | The accepted flit carries a valid DBID field. |
| `firstDBID` | This is the first DBID seen in the transaction. |

The base class uses these to update its internal DBID tracking.

---

## Step 7 — Implement DMT / DCT / DWT Support (Optional)

If your transaction supports Direct Memory Transfer (DMT), Direct Cache Transfer (DCT), or Direct Write Transfer (DWT), override the predicate and source methods:

```cpp
virtual bool IsDMTPossible() const noexcept override { return true; }

virtual const FiredResponseFlit<config>*
    GetDMTSrcIDSource(const Global<config>& glbl) const noexcept override
{
    // Return the first CompData flit, which carries the DMT SrcID.
    return this->GetFirstDAT({ Opcodes::DAT::CompData });
}

virtual const FiredResponseFlit<config>*
    GetDMTTgtIDSource(const Global<config>& glbl) const noexcept override
{
    return this->GetFirstDAT({ Opcodes::DAT::CompData });
}
```

Override `IsDCTPossible()` / `GetDCT*` and `IsDWTPossible()` / `GetDWT*` analogously for DCT and DWT respectively.  All three default to `false` / `nullptr` in the base class.

---

## Step 8 — Add Cache State Transitions

If the transaction changes cache coherence state, create a corresponding state-transition table.

**Files:** `chi/xact/chi_xact_state/cst_xact_read.hpp` (or `cst_xact_write.hpp`, `cst_xact_dataless.hpp`, `cst_xact_atomic.hpp`, `cst_xact_snoop.hpp`).

```cpp
namespace Transitions {

    // MyNewOpcode: I → UC after CompData_UC
    constexpr CacheStateTransition MyNewOpcode_I_to_UC = {
        CacheStateTransition(I, UC).TypeRead()
            .CompData(UC)
    };

    constexpr std::array MyNewOpcode = {
        MyNewOpcode_I_to_UC
    };
}
```

`CacheStateTransition` builder methods:

| Method | Purpose |
|--------|---------|
| `.TypeRead()` | Marks this as a read-type transition |
| `.TypeDataless()` | Marks this as a dataless-type transition |
| `.TypeWriteCopyBack()` | Marks this as a copy-back write transition |
| `.TypeWriteNonCopyBack()` | Non-copy-back write |
| `.TypeAtomic()` | Atomic transition |
| `.TypeSnoop()` / `.TypeSnoopForward()` | Snoop transitions |
| `.CompData(mask)` | Valid cache states after a CompData response |
| `.DataSepResp(mask)` | Valid states after a DataSepResp response |
| `.Comp(mask)` | Valid states after a Comp response |
| `.SnpResp(mask)` | Valid states after a SnpResp |
| `.SnpRespData(mask)` | Valid states after a SnpRespData |
| `.RetToSrc(rts)` | RetToSrc field expectation |
| `.DataPull(dp)` | DataPull expectation |

Cache states (`CacheState` bit-field shortcuts available in scope):

```
UC   – Unique Clean
UCE  – Unique Clean Exclusive
UD   – Unique Dirty
UDP  – Unique Dirty Persist
SC   – Shared Clean
SD   – Shared Dirty
I    – Invalid
```

Multiple initial states are combined with `|`, e.g., `I | UCE`.  The `CacheResp` type (response result) additionally supports `_PD` (Pass-Dirty) variants: `UC_PD`, `SC_PD`, etc.

After defining the transitions, reference them from the compile-time evaluator files:

- `cst_consteval_initials.hpp` — maps an opcode to its valid initial states.
- `cst_consteval_responses.hpp` — maps an opcode to its valid Comp/CompData/SnpResp responses.
- `cst_consteval_intermediates.hpp` — maps an opcode to intermediate-state expectations.

---

## Step 9 — Register the New Class in `chi_xactions_impl.hpp`

**File:** `chi/xact/chi_xactions/chi_xactions_impl.hpp`

Add a `#include` for the new implementation file:

```cpp
#include "chi_xactions_impl_MyNewTransaction.hpp"
```

---

## Step 10 — Expose Through the Joint Analyser (Optional)

`chi/xact/chi_joint.hpp` provides the `Joint` class used by tooling (e.g., `clog2log`) to decode and classify transactions from a stream of flits.  If you want the new transaction to be picked up automatically:

1. Open `chi/xact/chi_joint.hpp`.
2. Find the section where `Xaction*` constructors are tried in sequence.
3. Add a constructor attempt for `XactionMyNewTransaction<config>`.

---

## Step 11 — Write Tests

Tests live under `test/`.  Add a new test case that exercises at least:

- A correctly formed initial flit that is accepted (`firstDenial == XactDenial::ACCEPTED`).
- At least one valid completion path (`IsComplete()` returns `true`).
- An incorrectly formed initial flit that is rejected with the expected denial code.
- Any issue-version-specific paths, guarded by the matching `#ifdef`.

---

## Checklist

- [ ] `XactionType` enum extended with the new type.
- [ ] Implementation file `chi_xactions_impl_MyNewTransaction.hpp` created.
- [ ] Constructor validates channel, opcode whitelist, direction, and field mapping.
- [ ] `Clone()` / `CloneAsIs()` implemented.
- [ ] `IsTxnIDComplete()`, `IsDBIDComplete()`, `IsComplete()` implemented.
- [ ] `NextRSPNoRecord()` handles all expected RSP opcodes (and rejects everything else).
- [ ] `NextDATNoRecord()` handles all expected DAT opcodes (and rejects everything else).
- [ ] DMT/DCT/DWT predicates and source helpers overridden if applicable.
- [ ] Cache state transitions defined in the appropriate `cst_xact_*.hpp` and referenced from the consteval headers.
- [ ] New file included in `chi_xactions_impl.hpp`.
- [ ] Joint analyser updated if the transaction should be discoverable from flit streams.
- [ ] Tests written and passing.
- [ ] All issue-specific code wrapped in the appropriate `#ifdef CHI_ISSUE_*_ENABLE` guards.
