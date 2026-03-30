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

> **Note:** The file uses a repetition-guard idiom:
> ```
> #if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_PROTOCOL_ENCODING_B)) \
>  && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_PROTOCOL_ENCODING_EB))
> ```
> The guard ensures the block is processed exactly once per issue when both issue headers are included together.

---

## Step 3 — Add Node-Type Namespace Aliases (REQ and SNP only)

Some opcodes are valid only for specific node types.  The `chi/spec/chi_protocol_encoding.hpp` file contains per-node-type sub-namespaces that re-export the subset of valid opcodes.  Example sub-namespaces for REQ:

```
Opcodes::REQ::RN     – opcodes usable by any RN
Opcodes::REQ::RN_F   – opcodes usable by RN-F (full-cache requester)
Opcodes::REQ::RN_D   – opcodes usable by RN-D (DVM-only requester)
Opcodes::REQ::HN     – opcodes usable by HN
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

### 4b — Add a field-mapping constant in the appropriate `*FieldMappings` namespace

For a REQ opcode:

```cpp
namespace RequestFieldMappings {
    using enum FieldConvention;
    //  QoS TgtID SrcID TxnID Op  AllowR PCrd  RSVDC TagOp Trace MPAM  Addr  NS    Size  Alloc Cache Dev   EWA   SnpA  DoDWT Order LkSh  Excl  SnpMe ExpCA LPID  TGrpID SGrpID PGrpID RetNID StNID SLCHint StNIDV Endian Deep  RetTID StLPIDV StLPID
    inline constexpr RequestFieldMappingBack MyNewOpcode (Y , Y , Y , Y , Y , Y ,   Y ,  Y ,   Y ,  Y ,   Y ,  Y ,  Y ,  B64,  Y ,  Y ,  Y ,  Y ,  Y ,  S ,   Y ,  Y ,  Y ,  S ,   Y ,  Y ,   S ,   S ,   S ,   S ,   S ,   Y ,  I0,  I0,  I0,  I0,  I0,  I0);
}
```

The constructor argument order matches the fields of `RequestFieldMappingBack`, declared earlier in the same file.  Use existing opcodes as templates and adjust only the fields that differ.

For RSP opcodes use `ResponseFieldMappings::ResponseFieldMappingBack`, for DAT use `DataFieldMappingBack`, for SNP use `SnoopFieldMappingBack`.  Each has a different set of fields matching the corresponding channel's flit layout.

### 4c — Register the mapping in the constructor of the mapping table

Locate the constructor implementation in the same file (`RequestFieldMappingTable::RequestFieldMappingTable()`) and add one line:

```cpp
table[Opcodes::REQ::MyNewOpcode] = &RequestFieldMappings::MyNewOpcode;
```

Wrap in the appropriate `#ifdef` block when the opcode is issue-specific:

```cpp
#ifdef CHI_ISSUE_EB_ENABLE
    table[Opcodes::REQ::MyNewOpcode] = &RequestFieldMappings::MyNewOpcode;
#endif
```

---

## Step 5 — Integrate with Transaction Implementations

An opcode that starts a new transaction type needs to be recognised by a `Xaction*` constructor.  An opcode that is a *response* to an existing transaction type needs to be handled inside `NextRSPNoRecord()` or `NextDATNoRecord()`.

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

Refer to the *existing* opcode list in the same constructor to understand the convention.

### 5b — Handling a new RSP or DAT opcode in `NextRSPNoRecord` / `NextDATNoRecord`

These virtual methods are called when the transaction absorbs a subsequent flit.  Add a branch for the new opcode and return `XactDenial::ACCEPTED` on success or an appropriate `XactDenial::DENIED_*` constant on failure.

```cpp
virtual XactDenialEnum NextRSPNoRecord(
    const Global<config>& glbl,
    const FiredResponseFlit<config>& rspFlit,
    bool& hasDBID, bool& firstDBID) noexcept override
{
    // … existing branches …

#ifdef CHI_ISSUE_EB_ENABLE
    if (rspFlit.flit.rsp.Opcode() == Opcodes::RSP::MyNewRSPOpcode)
    {
        if (!rspFlit.IsToRequester(glbl))
            return XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN;
        // Additional validation …
        return XactDenial::ACCEPTED;
    }
#endif

    return XactDenial::DENIED_RSP_OPCODE;
}
```

---

## Step 6 — Define or Update Cache State Transitions

If the new opcode affects cache coherence state, update the relevant cache state transition table.

**Files:** `chi/xact/chi_xact_state/cst_xact_read.hpp`, `cst_xact_write.hpp`, `cst_xact_dataless.hpp`, `cst_xact_atomic.hpp`, `cst_xact_snoop.hpp`.

A `CacheStateTransition` describes:
- `initialExpectedState` — the cache state the requestor is expected to hold before the request.
- `initialPermittedState` — additional states also allowed.
- `finalState` — the cache state after the transaction completes.
- Response fields (`respCompData`, `respDataSepResp`, etc.) — bit-masks of valid `Resp` encodings.

```cpp
namespace Transitions {
    // MyNewOpcode: I → SC on CompData_SC
    constexpr CacheStateTransition MyNewOpcode_I_to_SC = {
        CacheStateTransition(I, SC).TypeRead()
            .CompData(SC)
            .DataSepResp(SC)
    };

    constexpr std::array MyNewOpcode = {
        MyNewOpcode_I_to_SC
    };
}
```

After defining the transitions, reference them from the compile-time evaluator in `cst_consteval_initials.hpp`, `cst_consteval_responses.hpp`, and `cst_consteval_intermediates.hpp` so that the state-machine can validate transactions against the new opcode.

---

## Step 7 — Handle Issue-Specific Overrides in `chi_b/`, `chi_c/`, `chi_eb/`

If the opcode only exists in a specific issue and the encoding file in `chi/spec/` does not already include it under the appropriate `#ifdef`, check whether a per-issue override file is more appropriate.  The directories `chi_b/`, `chi_c/`, and `chi_eb/` each contain small header files that override or extend definitions from the base `chi/` directory.

---

## Checklist

- [ ] Opcode constant declared in `chi/spec/chi_protocol_encoding.hpp` with correct numeric value and `#ifdef` guard.
- [ ] Node-type alias added to the relevant sub-namespaces (`RN`, `RN_F`, `HN`, …) if applicable.
- [ ] Field mapping constant added to the appropriate `*FieldMappings` namespace in `chi_xact_field_eb.hpp`.
- [ ] Field mapping registered in the `*FieldMappingTable` constructor.
- [ ] Opcode recognised in the constructor of the relevant `Xaction*` class (for REQ/SNP first-flits).
- [ ] `NextRSPNoRecord` / `NextDATNoRecord` updated (for RSP/DAT subsequent flits).
- [ ] Cache state transitions added or updated in `cst_xact_*.hpp` and referenced from the consteval headers.
- [ ] Compile-time evaluator files updated if required.
- [ ] All changes wrapped in the appropriate issue-version `#ifdef` guard.
