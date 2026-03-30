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
├── XactionAllocatingRead<config>       – ReadShared / ReadClean / ReadUnique / …
├── XactionNonAllocatingRead<config>    – ReadOnce / ReadNoSnp / ReadOnceCleanInvalid / …
├── XactionImmediateWrite<config>       – WriteUniqueFull / WriteNoSnpFull / …
├── XactionWriteZero<config>            – WriteUniqueZero / WriteNoSnpZero
├── XactionCopyBackWrite<config>        – WriteBackFull / WriteCleanFull / …
├── XactionAtomic<config>               – AtomicStore / AtomicLoad / AtomicSwap / AtomicCompare
├── XactionIndependentStash<config>     – StashOnceShared / StashOnceUnique / …
├── XactionDataless<config>             – CleanShared / CleanInvalid / MakeInvalid / Evict / …
├── XactionHomeRead<config>             – HN-side read (RXREQ from RN)
├── XactionHomeWrite<config>            – HN-side write
├── XactionHomeWriteZero<config>        – HN-side write-zero
├── XactionHomeDataless<config>         – HN-side dataless
├── XactionHomeAtomic<config>           – HN-side atomic
├── XactionHomeSnoop<config>            – SnpShared / SnpClean / SnpUnique / … (non-fwd)
├── XactionForwardSnoop<config>         – SnpSharedFwd / SnpCleanFwd / SnpUniqueFwd / …
└── XactionDVMSnoop<config>             – SnpDVMOp
```

All implementation files live in `chi/xact/chi_xactions/`.

---

## Architecture: how `RNFJoint` dispatches flits to transactions

Understanding the `Joint` dispatch pipeline helps you place your new code correctly.

`RNFJoint<config>` (and `SNFJoint<config>`) maintain two transaction stores indexed by TxnID:

| Store | Field | Purpose |
|---|---|---|
| `txTransactions` | TX side | REQ flits sent by this node (requester perspective) |
| `rxTransactions` | RX side | SNP flits received (snooped) by this node |
| `txDBIDTransactions` | TX DBID | Write-phase tracking by DBID |
| `txDBIDOverlappableTransactions` | TX overlappable | Write transactions that allow DBID reuse |

When a flit arrives, the Joint calls one of `NextTXREQ`, `NextRXSNP`, `NextTXRSP`, `NextRXRSP`, `NextTXDAT`, or `NextRXDAT`.  Each method:

1. Looks up an existing transaction in the appropriate store by TxnID.
2. If this is a first flit (REQ/SNP), runs the opcode decoder to instantiate the correct `Xaction*` subclass via a `Construct*` factory method.
3. Calls `xaction->NextRSP(...)` or `xaction->NextDAT(...)` on the matched transaction.
4. On `ACCEPTED`, fires the appropriate `Gravity::EventBus` event (`OnAccepted`, `OnComplete`, etc.).

The event bus events are:

| Event type | Fired when |
|---|---|
| `OnAccepted` | First flit is accepted and transaction is opened |
| `OnRetry` | Transaction receives RetryAck (must be resent) |
| `OnTxnIDAllocation` | A TxnID slot is consumed |
| `OnTxnIDFree` | A TxnID slot is released (`IsTxnIDComplete()` → `true`) |
| `OnDBIDAllocation` | A DBID slot is consumed |
| `OnDBIDFree` | A DBID slot is released (`IsDBIDComplete()` → `true`) |
| `OnComplete` | Transaction is fully complete (`IsComplete()` → `true`) |
| `OnDeniedRequest` | A request flit was rejected |
| `OnDeniedResponse` | A response flit was rejected |

External scoreboard code subscribes to these events to track transaction lifecycle.

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


// CHIron keeps `namespace CHI` commented out in every translation unit
// to allow the headers to be included without wrapping the caller's code
// in an extra namespace scope.  Follow the same convention here.
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
    //    For REQ-initiated transactions (most read/write/dataless):
    if (!this->first.IsREQ())
    {
        this->firstDenial = XactDenial::DENIED_CHANNEL_NOT_REQ;
        return;
    }
    //    For SNP-initiated transactions (snoops), replace the above with:
    //      if (!this->first.IsSNP()) {
    //          this->firstDenial = XactDenial::DENIED_CHANNEL_NOT_SNP;
    //          return;
    //      }

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

### Available direction-checking helpers

**On `FiredRequestFlit<config>` (for the initiating REQ or SNP):**

| Method | Meaning |
|---|---|
| `IsFromRequesterToHome(glbl)` | REQ from RN to HN (most read/write/dataless) |
| `IsFromRequesterToSubordinate(glbl)` | REQ from RN to SN (non-snoop reads/writes to SN) |
| `IsFromHomeToRequester(glbl)` | SNP from HN to RN |
| `IsFromHomeToSubordinate(glbl)` | REQ/SNP from HN to SN |
| `IsFromSubordinateToHome(glbl)` | REQ from SN to HN |
| `IsFromSubordinateToRequester(glbl)` | DAT/RSP from SN to RN |

**On `FiredResponseFlit<config>` (for subsequent RSP/DAT flits):**

| Method | Meaning |
|---|---|
| `IsToRequester(glbl)` | RSP/DAT destined for RN |
| `IsToHome(glbl)` | RSP/DAT destined for HN (e.g. CompAck, write data) |
| `IsToSubordinate(glbl)` | RSP/DAT destined for SN |
| `IsFromRequester(glbl)` | Flit originates from RN |
| `IsFromHome(glbl)` | Flit originates from HN |
| `IsFromSubordinate(glbl)` | Flit originates from SN |
| `IsFromRequesterToHome(glbl)` | RN→HN (e.g. CompAck, write data) |
| `IsFromHomeToRequester(glbl)` | HN→RN (e.g. Comp, CompData) |
| `IsFromHomeToSubordinate(glbl)` | HN→SN |
| `IsFromSubordinateToRequester(glbl)` | SN→RN (e.g. DMT CompData) |
| `IsFromSubordinateToHome(glbl)` | SN→HN |
| `IsToRequesterDMT(glbl)` | CompData on DMT path (SN→RN, directed) |
| `IsToRequesterDCT(glbl)` | CompData on DCT path (RN→RN, directed) |
| `IsToRequesterDWT(glbl)` | Write data on DWT path |

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

`Clone()` is called by `RNFJoint::Fork()` when the joint is deep-copied (e.g., to checkpoint state for branch prediction or error recovery).

---

## Step 5 — Implement Completion Predicates

Three pure-virtual predicates must be implemented.  They are called by `RNFJoint` to determine when to free resources and fire lifecycle events.

### Semantics

| Method | Controls | Fires event |
|---|---|---|
| `IsComplete()` | Whether the transaction is fully done | `OnComplete` |
| `IsTxnIDComplete()` | Whether the TxnID slot can be released at the home node | `OnTxnIDFree` |
| `IsDBIDComplete()` | Whether the DBID slot can be released | `OnDBIDFree` |

For simple transactions (read, dataless), all three coincide:

```cpp
template<FlitConfigurationConcept config>
inline bool XactionMyNewTransaction<config>::IsTxnIDComplete(
    const Global<config>& glbl) const noexcept
{
    return IsComplete(glbl);
}

template<FlitConfigurationConcept config>
inline bool XactionMyNewTransaction<config>::IsDBIDComplete(
    const Global<config>& glbl) const noexcept
{
    return IsComplete(glbl);
}

template<FlitConfigurationConcept config>
inline bool XactionMyNewTransaction<config>::IsComplete(
    const Global<config>& glbl) const noexcept
{
    return this->HasRSP({ Opcodes::RSP::Comp });
}
```

### Write transaction example (separate release points)

For write transactions the DBID is allocated when the home sends `CompDBIDResp` and is not freed
until the requester sends the data back.  The TxnID is freed earlier (once `Comp` arrives):

```cpp
// IsTxnIDComplete: home freed our TxnID once we sent all write data
template<FlitConfigurationConcept config>
inline bool XactionMyNewTransaction<config>::IsTxnIDComplete(
    const Global<config>& glbl) const noexcept
{
    // Comp or CompDBIDResp from home acknowledges TxnID release
    return this->HasRSP({ Opcodes::RSP::Comp })
        || this->HasRSP({ Opcodes::RSP::CompDBIDResp });
}

// IsDBIDComplete: DBID freed once we sent the write data
template<FlitConfigurationConcept config>
inline bool XactionMyNewTransaction<config>::IsDBIDComplete(
    const Global<config>& glbl) const noexcept
{
    return this->HasDAT({ Opcodes::DAT::NonCopyBackWrData })
        || this->HasDAT({ Opcodes::DAT::NCBWrDataCompAck })
        || this->HasDAT({ Opcodes::DAT::WriteDataCancel });
}

// IsComplete: entire exchange finished
template<FlitConfigurationConcept config>
inline bool XactionMyNewTransaction<config>::IsComplete(
    const Global<config>& glbl) const noexcept
{
    return IsTxnIDComplete(glbl) && IsDBIDComplete(glbl);
}
```

### Available query methods on `Xaction<config>`

```cpp
// Check if any accepted flit in the subsequence has one of the given opcodes
bool HasRSP(std::initializer_list<typename Flits::RSP<config>::opcode_t>) const noexcept;
bool HasDAT(std::initializer_list<typename Flits::DAT<config>::opcode_t>) const noexcept;

// Retrieve specific flits from the subsequence
const FiredResponseFlit<config>* GetFirstRSP()  const noexcept;
const FiredResponseFlit<config>* GetFirstDAT()  const noexcept;
const FiredResponseFlit<config>* GetLastRSP()   const noexcept;
const FiredResponseFlit<config>* GetLastDAT()   const noexcept;

// Retrieve specific flits by opcode filter
const FiredResponseFlit<config>* GetFirstRSP(std::initializer_list<…opcode_t>) const noexcept;
const FiredResponseFlit<config>* GetFirstDAT(std::initializer_list<…opcode_t>) const noexcept;
const FiredResponseFlit<config>* GetLastRSP (std::initializer_list<…opcode_t>) const noexcept;
const FiredResponseFlit<config>* GetLastDAT (std::initializer_list<…opcode_t>) const noexcept;

// Retrieve flits filtered by both opcode and direction (From/To + scope)
const FiredResponseFlit<config>* GetFirstRSPFrom(const Global<config>&, XactScopeEnum) const noexcept;
const FiredResponseFlit<config>* GetFirstDATFrom(const Global<config>&, XactScopeEnum) const noexcept;
// … GetLastRSPFrom, GetLastDATFrom, GetFirstRSPTo, GetFirstDATTo, GetLastRSPTo, GetLastDATTo …

// DBID tracking
bool GotDBID() const noexcept;
std::optional<typename Flits::RSP<config>::dbid_t> GetDBID() const noexcept;
const FiredResponseFlit<config>* GetDBIDSource() const noexcept;
```

---

## Step 6 — Implement `NextRSPNoRecord` and `NextDATNoRecord`

These methods are called by `Joint::NextTXRSP` / `NextRXRSP` / `NextTXDAT` / `NextRXDAT` each time a RSP or DAT flit arrives that matches the transaction's TxnID.  They must either accept the flit (return `XactDenial::ACCEPTED`) or reject it with a specific denial reason.

The base class records the flit into `this->subsequence` **only if** you return `ACCEPTED`; the prefix `NoRecord` in the method name refers to the fact that you receive the flit *before* it has been appended.  Never manually append to `this->subsequence`.

### `NextRSPNoRecord` skeleton

```cpp
template<FlitConfigurationConcept config>
inline XactDenialEnum XactionMyNewTransaction<config>::NextRSPNoRecord(
    const Global<config>&               glbl,
    const FiredResponseFlit<config>&    rspFlit,
    bool& hasDBID, bool& firstDBID) noexcept
{
    // 1. Guard: no more flits once the transaction is logically complete.
    if (IsComplete(glbl))
        return XactDenial::DENIED_COMPLETED_RSP;

    if (!rspFlit.IsRSP())
        return XactDenial::DENIED_CHANNEL_NOT_RSP;

    // 2. RetryAck and PCrdGrant are handled uniformly by all non-snoop transactions.
    if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::RetryAck)
    {
        // AllowRetry enforcement is left to external scoreboards.
        hasDBID = false;
        return XactDenial::ACCEPTED;
    }

    if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::PCrdGrant)
    {
        hasDBID = false;
        return XactDenial::ACCEPTED;
    }

    // 3. Transaction-specific RSP handling.
    if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::Comp)
    {
        if (!rspFlit.IsToRequester(glbl))
            return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;

        if (this->HasRSP({ Opcodes::RSP::Comp }))
            return XactDenial::DENIED_COMP_AFTER_COMP;

        if (rspFlit.flit.rsp.TxnID() != this->first.flit.req.TxnID())
            return XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ;

        // Signal whether this RSP carries a DBID.
        hasDBID   = (rspFlit.flit.rsp.DBID() != 0);
        firstDBID = hasDBID && !this->GotDBID();
        return XactDenial::ACCEPTED;
    }

    return XactDenial::DENIED_RSP_OPCODE;
}
```

### `NextDATNoRecord` skeleton

```cpp
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

    // Example: accept a CompData response.
    if (datFlit.flit.dat.Opcode() == Opcodes::DAT::CompData)
    {
        if (!datFlit.IsToRequester(glbl))
            return XactDenial::DENIED_DAT_NOT_FROM_HN_TO_RN;

        if (datFlit.flit.dat.TxnID() != this->first.flit.req.TxnID())
            return XactDenial::DENIED_DAT_TXNID_MISMATCHING_REQ;

        hasDBID   = (datFlit.flit.dat.DBID() != 0);
        firstDBID = hasDBID && !this->GotDBID();
        return XactDenial::ACCEPTED;
    }

    // If this transaction type does not carry data, reject all other DAT flits.
    return XactDenial::DENIED_DAT_OPCODE;
}
```

### `hasDBID` / `firstDBID` output parameters

| Parameter | Set to `true` when … |
|---|---|
| `hasDBID` | The accepted flit carries a non-zero DBID field |
| `firstDBID` | This is the **first** DBID seen in this transaction (used by `RNFJoint` to decide whether to insert the transaction into `txDBIDTransactions`) |

### Commonly used denial codes

**RSP denial codes:**

| Code | Meaning |
|---|---|
| `DENIED_COMPLETED_RSP` | Transaction already complete |
| `DENIED_CHANNEL_NOT_RSP` | Flit is not on RSP channel |
| `DENIED_RSP_OPCODE` | RSP opcode not expected |
| `DENIED_RSP_NOT_FROM_HN_TO_RN` | Wrong direction (expected HN→RN) |
| `DENIED_RSP_NOT_FROM_RN_TO_HN` | Wrong direction (expected RN→HN, e.g. CompAck) |
| `DENIED_RSP_NOT_FROM_SN_TO_HN` | Wrong direction for SN-origin RSP |
| `DENIED_RSP_TXNID_MISMATCHING_REQ` | TxnID field mismatch |
| `DENIED_RSP_TGTID_MISMATCHING_REQ` | TgtID field mismatch |
| `DENIED_COMP_AFTER_COMP` | Duplicate Comp |
| `DENIED_COMPACK_AFTER_COMPACK` | Duplicate CompAck |
| `DENIED_COMPACK_BEFORE_COMP` | CompAck arrived before Comp |

**DAT denial codes:**

| Code | Meaning |
|---|---|
| `DENIED_COMPLETED_DAT` | Transaction already complete |
| `DENIED_CHANNEL_NOT_DAT` | Flit is not on DAT channel |
| `DENIED_DAT_OPCODE` | DAT opcode not expected |
| `DENIED_DAT_NOT_FROM_HN_TO_RN` | Expected HN→RN data |
| `DENIED_DAT_NOT_FROM_RN_TO_HN` | Expected RN→HN data |
| `DENIED_DAT_NOT_FROM_SN_TO_HN` | Expected SN→HN data |
| `DENIED_DAT_TXNID_MISMATCHING_REQ` | TxnID mismatch |

---

## Step 7 — Implement DMT / DCT / DWT Support (Optional)

If your transaction supports Direct Memory Transfer (DMT), Direct Cache Transfer (DCT), or Direct Write Transfer (DWT), override the predicate and source methods.

### DMT — Direct Memory Transfer (SN → RN, bypassing HN)

```cpp
virtual bool IsDMTPossible() const noexcept override { return true; }

// Returns the flit in the subsequence that carries the DMT SrcID
virtual const FiredResponseFlit<config>*
    GetDMTSrcIDSource(const Global<config>& glbl) const noexcept override
{
    // CompData from the subordinate carries the DMT SrcID
    return this->GetFirstDAT({ Opcodes::DAT::CompData });
}

// Returns the flit that carries the DMT TgtID (usually the same CompData)
virtual const FiredResponseFlit<config>*
    GetDMTTgtIDSource(const Global<config>& glbl) const noexcept override
{
    return this->GetFirstDAT({ Opcodes::DAT::CompData });
}
```

### DCT — Direct Cache Transfer (RN → RN, home-initiated forwarding)

```cpp
virtual bool IsDCTPossible() const noexcept override { return true; }

virtual const FiredResponseFlit<config>*
    GetDCTSrcIDSource(const Global<config>& glbl) const noexcept override
{
    // SnpRespDataFwded from the snoop target carries the DCT SrcID
    return this->GetFirstDAT({ Opcodes::DAT::SnpRespDataFwded });
}

virtual const FiredResponseFlit<config>*
    GetDCTTgtIDSource(const Global<config>& glbl) const noexcept override
{
    return this->GetFirstDAT({ Opcodes::DAT::CompData });
}
```

### DWT — Direct Write Transfer

```cpp
virtual bool IsDWTPossible() const noexcept override { return true; }

virtual const FiredResponseFlit<config>*
    GetDWTSrcIDSource(const Global<config>& glbl) const noexcept override
{
    return this->GetFirstDAT({ Opcodes::DAT::NonCopyBackWrData });
}

virtual const FiredResponseFlit<config>*
    GetDWTTgtIDSource(const Global<config>& glbl) const noexcept override
{
    return this->GetFirstDAT({ Opcodes::DAT::NonCopyBackWrData });
}
```

All six methods default to `false` / `nullptr` in the base class.

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
            .DataSepResp(UC)
    };

    // MyNewOpcode: SC → UC after CompData_UC (silent sharing permitted)
    constexpr CacheStateTransition MyNewOpcode_SC_to_UC = {
        CacheStateTransition(SC, UC).TypeRead()
            .CompData(UC)
            .DataSepResp(UC)
    };

    constexpr std::array MyNewOpcode = {
        MyNewOpcode_I_to_UC,
        MyNewOpcode_SC_to_UC
    };
}
```

`CacheStateTransition` builder methods — see Step 6 of the opcode guide for the full table.

After defining the transitions, wire them into the three consteval files:

### `cst_consteval_initials.hpp`

```cpp
// namespace Initials { … }
constexpr CacheState MyNewOpcode = _MR_General(MyNewOpcode);   // read/dataless/snoop
// OR
constexpr CacheState MyNewOpcode = _MR_Write(MyNewOpcode);     // write
```

### `cst_consteval_responses.hpp`

Only forward-snoop transactions need an entry:

```cpp
// namespace Responses { … }
constexpr details::TableR0 MyNewSnpFwdOpcode = _Table_DCT(MyNewSnpFwdOpcode);
```

### `cst_consteval_intermediates.hpp`

Only snoops that involve a nested/forwarded intermediate state need an entry here.  Follow the existing `SnpSharedFwd` / `SnpUniqueFwd` patterns.

---

## Step 9 — Register the New Class in `chi_xactions_impl.hpp`

**File:** `chi/xact/chi_xactions/chi_xactions_impl.hpp`

Add a `#include` for the new implementation file:

```cpp
#include "chi_xactions_impl_MyNewTransaction.hpp"
```

---

## Step 10 — Expose Through the Joint Analyser

**File:** `chi/xact/chi_joint.hpp`

The `RNFJoint` constructor maps opcodes to factory methods using two macros:

```cpp
#define SET_REQ_XACTION(opcode, type) \
    reqDecoder[Opcodes::REQ::opcode].SetCompanion(&RNFJoint<config>::Construct##type)

#define SET_SNP_XACTION(opcode, type) \
    snpDecoder[Opcodes::SNP::opcode].SetCompanion(&RNFJoint<config>::Construct##type)
```

### 10a — Add a `Construct*` factory method to `RNFJoint`

Each transaction type needs a protected factory method:

```cpp
// In class RNFJoint (in the class definition section):
protected:
    std::shared_ptr<Xaction<config>> ConstructMyNewTransaction(
        const Global<config>&             glbl,
        const FiredRequestFlit<config>&   reqFlit,
        std::shared_ptr<Xaction<config>>  retried) noexcept;
```

And its implementation (in the implementation section of the same file):

```cpp
template<FlitConfigurationConcept config>
inline std::shared_ptr<Xaction<config>> RNFJoint<config>::ConstructMyNewTransaction(
    const Global<config>&             glbl,
    const FiredRequestFlit<config>&   reqFlit,
    std::shared_ptr<Xaction<config>>  retried) noexcept
{
    return std::make_shared<XactionMyNewTransaction<config>>(glbl, reqFlit, retried);
}
```

### 10b — Register the opcode-to-factory mapping

In `RNFJoint<config>::RNFJoint()` (the default constructor), inside the `SET_REQ_XACTION` block:

```cpp
SET_REQ_XACTION(MyNewOpcode, MyNewTransaction);  // 0xXX
```

For a SNP-initiated transaction, use `SET_SNP_XACTION` instead:

```cpp
SET_SNP_XACTION(MyNewSNPOpcode, MyNewTransaction);  // 0xXX
```

The `type` suffix must exactly match the suffix you used in `Construct##type`.  Using `None` means the opcode is recognised but no transaction object is created (used for credit-return opcodes).

---

## Step 11 — Write Tests

Tests live under `test/tc_chi_xact_state/src/`.  The test framework uses a fluent builder API (`TCPtInitial` for initial-state tests, `TCPtResp` for response-state tests).

### 11a — Initial-state test (`TCPtInitial`)

Add a function declaration in a new header `tcpt_x_initial_my_transaction.hpp`:

```cpp
void TCPtXInitialMyTransaction(
    size_t*                     totalCount,
    size_t*                     errCountFail,
    size_t*                     errCountEnvError,
    std::vector<std::string>*   errList,
    const Xact::Topology&       topo) noexcept;
```

Implement it in `tcpt_x_initial_my_transaction_e.cpp` (or `_b.cpp` for Issue B):

```cpp
#include "tc_common_initial.hpp"
#include "tcpt_x_initial_my_transaction.hpp"

#ifdef CHI_ISSUE_E

void TCPtXInitialMyTransaction(
    size_t* totalCount, size_t* errCountFail,
    size_t* errCountEnvError, std::vector<std::string>* errList,
    const Xact::Topology& topo) noexcept
{
    TCPtInitial* tcpt = (new TCPtInitial())
        ->TotalCount(totalCount)
        ->ErrCountFail(errCountFail)
        ->ErrCountEnvError(errCountEnvError)
        ->ErrList(errList)
        ->Topology(topo)
        ->Begin();

    // Test all initial states for the opcode.
    // Third argument is 'accept': true = this initial state is valid for the opcode.
    tcpt
        ->ForkREQ("# TCPt X.1. Initial state of MyNewOpcode", Opcodes::REQ::MyNewOpcode)

            ->ForkSilentTransitions("## X.1.1. No silent transitions", false, false, false, false)
                ->Leaf("### X.1.1.1. UC  -> MyNewOpcode", Xact::CacheStates::UC , false)
                ->Leaf("### X.1.1.2. UCE -> MyNewOpcode", Xact::CacheStates::UCE, false)
                ->Leaf("### X.1.1.3. UD  -> MyNewOpcode", Xact::CacheStates::UD , false)
                ->Leaf("### X.1.1.4. UDP -> MyNewOpcode", Xact::CacheStates::UDP, false)
                ->Leaf("### X.1.1.5. SC  -> MyNewOpcode", Xact::CacheStates::SC , false)
                ->Leaf("### X.1.1.6. SD  -> MyNewOpcode", Xact::CacheStates::SD , false)
                ->Leaf("### X.1.1.7. I   -> MyNewOpcode", Xact::CacheStates::I  , true )
            ->Rest()

        ->End();
}

#endif
```

`ForkSilentTransitions` parameters are `(title, silentEviction, silentSharing, silentStore, silentInvalidation)`.  Repeat the `ForkSilentTransitions` / `Leaf` blocks for every combination of silent-transition flags that changes the acceptance outcome.

### 11b — Response-state test (`TCPtResp`)

Add a function declaration in `tcpt_y_resp_my_transaction.hpp` and implement in `_e.cpp`:

```cpp
#include "tc_common_resp.hpp"
#include "tcpt_y_resp_my_transaction.hpp"

#ifdef CHI_ISSUE_E

void TCPtYRespMyTransaction(
    size_t* totalCount, size_t* errCountFail,
    size_t* errCountEnvError, std::vector<std::string>* errList,
    const Xact::Topology& topo) noexcept
{
    TCPtResp* tcpt = (new TCPtResp())
        ->TotalCount(totalCount)
        ->ErrCountFail(errCountFail)
        ->ErrCountEnvError(errCountEnvError)
        ->ErrList(errList)
        ->Topology(topo)
        ->Begin();

    tcpt
        ->ForkREQ("# TCPt Y.1. Response of MyNewOpcode", Opcodes::REQ::MyNewOpcode)

            ->TestAndForkInitial("## Y.1.1. I -> MyNewOpcode", Xact::CacheStates::I)
                ->ForkCompData("### Y.1.1.1. I -> MyNewOpcode -> CompData")
                    ->LeafRepeat("#### Y.1.1.1.1. I -> CompData_UC -> UC", Resps::CompData_UC, Xact::CacheStates::UC, true )
                    ->LeafRepeat("#### Y.1.1.1.2. I -> CompData_SC -> SC", Resps::CompData_SC, Xact::CacheStates::SC, true )
                    ->LeafRepeat("#### Y.1.1.1.3. I -> CompData_I  (invalid)", Resps::CompData_I,  Xact::CacheStates::None, false)
                ->Rest()
            ->Rest()

        ->End();
}

#endif
```

The `TCPtResp` builder offers fork methods that match every type of response flit:

| Fork method | Creates a branch for |
|---|---|
| `ForkCompData(title)` | CompData DAT |
| `ForkDataSepResp(title)` | DataSepResp DAT |
| `ForkComp(title)` | Comp RSP |
| `ForkCompStashDone(title)` | CompStashDone RSP |
| `ForkCompPersist(title)` | CompPersist RSP |
| `ForkCompDBIDResp(title)` | CompDBIDResp RSP |
| `ForkInstantComp(title, resp)` | Comp with a specific Resp field |
| `ForkInstantCompAndDBIDResp(title, resp)` | Comp + DBIDResp in one step |
| `ForkInstantDBIDResp(title)` | DBIDResp RSP |
| `ForkCopyBackWrData(title)` | CopyBackWrData DAT |
| `ForkNonCopyBackWrData(title)` | NonCopyBackWrData DAT |
| `ForkNCBWrDataCompAck(title)` | NCBWrDataCompAck DAT |
| `ForkWriteDataCancel(title)` | WriteDataCancel DAT |

`LeafRepeat` takes `(title, resp_encoding, expected_final_cache_state, accept)`.

### 11c — Register the test in `main.cpp`

```cpp
// In test/tc_chi_xact_state/src/main.cpp
#include "tcpt_x_initial_my_transaction.hpp"
#include "tcpt_y_resp_my_transaction.hpp"

// In the main test runner:
TCPtXInitialMyTransaction(&totalCount, &errCountFail, &errCountEnvError, &errList, topo);
TCPtYRespMyTransaction   (&totalCount, &errCountFail, &errCountEnvError, &errList, topo);
```

---

## Checklist

- [ ] `XactionType` enum extended with the new type.
- [ ] Implementation file `chi_xactions_impl_MyNewTransaction.hpp` created with the correct issue-guard boilerplate.
- [ ] Constructor validates: channel, opcode whitelist, direction, field mapping.
- [ ] `Clone()` / `CloneAsIs()` implemented.
- [ ] `IsTxnIDComplete()`, `IsDBIDComplete()`, `IsComplete()` implemented; semantics match the transaction's lifecycle.
- [ ] `NextRSPNoRecord()` handles all expected RSP opcodes (including RetryAck / PCrdGrant) and rejects everything else with the correct denial code.
- [ ] `NextDATNoRecord()` handles all expected DAT opcodes and rejects everything else.
- [ ] `hasDBID` / `firstDBID` correctly set for every accepted flit.
- [ ] DMT/DCT/DWT predicates and source helpers overridden if applicable.
- [ ] Cache state transitions defined in the appropriate `cst_xact_*.hpp`.
- [ ] Transition wired into `cst_consteval_initials.hpp` with `_MR_General` / `_MR_Write`.
- [ ] Transition wired into `cst_consteval_responses.hpp` (forwarding snoops only).
- [ ] Transition wired into `cst_consteval_intermediates.hpp` (nested-state snoops only).
- [ ] New file included in `chi_xactions_impl.hpp`.
- [ ] `Construct*` method added to `RNFJoint` and `SET_REQ_XACTION` / `SET_SNP_XACTION` entry added in the constructor.
- [ ] `TCPtInitial` test added and registered in `main.cpp`.
- [ ] `TCPtResp` test added and registered in `main.cpp`.
- [ ] All issue-specific code wrapped in the appropriate `#ifdef CHI_ISSUE_*_ENABLE` guards.
