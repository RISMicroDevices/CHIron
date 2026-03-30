# CHIron Architecture Overview

CHIron is the world's first open-source **AMBA CHI (Coherent Hub Interface) infrastructure library**, providing protocol-level and transaction-level tooling for chip verification, simulation, and analysis. It is a header-only C++20 library used in production by the [OpenXiangShan](https://github.com/OpenXiangShan) project.

---

## Repository Layout

```
CHIron/
├── chi/          Core CHI Issue E implementation
├── chi_b/        CHI Issue B overrides
├── chi_c/        CHI Issue C overrides
├── chi_eb/       CHI Issue Eb overrides
├── clog/         CHI Transaction Log format (binary + text)
├── common/       Shared utilities (EventBus, ConcurrentQueue, …)
├── app/          Command-line analysis tools
├── test/         Transaction-state test suite
├── docs/         ← You are here
├── README.md
└── ERRATA.md
```

```mermaid
graph TD
    subgraph Core["Core CHI Library"]
        CHI["chi/\nIssue E (primary)"]
        CHI_B["chi_b/\nIssue B"]
        CHI_C["chi_c/\nIssue C"]
        CHI_EB["chi_eb/\nIssue Eb"]
    end

    subgraph Infra["Shared Infrastructure"]
        COMMON["common/\nEventBus · ConcurrentQueue\nStringAppender · Utilities"]
        CLOG["clog/\nTransaction Log\n(Binary + Text)"]
    end

    subgraph Apps["Command-Line Tools (app/)"]
        XSDB2CLOG["xsdb2clog"]
        CLOG2LOG["clog2log"]
        CLOG2COV["clog2coverage"]
        CLOG2CFG["clog2mkcfg"]
        RPT["report_flit"]
    end

    subgraph Tests["Test Suite (test/)"]
        TC["tc_chi_xact_state\n(state transition tests)"]
    end

    CHI --> COMMON
    CHI --> CLOG
    CHI_B & CHI_C & CHI_EB --> CHI
    Apps --> CLOG
    Apps --> CHI
    Tests --> CHI
```

---

## Architectural Layers

CHIron is organised as a strict four-layer stack. Higher layers build on lower ones; lower layers have no dependencies on higher layers.

```mermaid
graph BT
    L1["Layer 1 — Flit / Protocol\nBit-accurate flit structs, field encodings,\nopcode tables for REQ · RSP · DAT · SNP"]
    L2["Layer 2 — Channel / Link\nTX/RX channel pairs, interface types\n(RN-F, HN-F, SN-F, …), link definitions"]
    L3["Layer 3 — Transaction (Xact)\nHigh-level Xaction objects, state tables,\ncache-state tracking, denial detection"]
    L4["Layer 4 — Joint (System)\nSystem-wide transaction co-ordination,\nevent-driven denial reporting, topology"]

    L1 --> L2 --> L3 --> L4

    style L1 fill:#d4e6f1,stroke:#2980b9
    style L2 fill:#d5f5e3,stroke:#27ae60
    style L3 fill:#fdebd0,stroke:#e67e22
    style L4 fill:#f9ebea,stroke:#c0392b
```

| Layer | Namespace | Key files |
|-------|-----------|-----------|
| Flit / Protocol | `CHI::Flits` | `chi/spec/chi_protocol_flits.hpp`, `chi_protocol_encoding.hpp` |
| Channel / Link | `CHI::Links` | `chi/spec/chi_link_channels.hpp`, `chi_link_interfaces.hpp`, `chi_link_links.hpp` |
| Transaction | `CHI::Xact` | `chi/xact/chi_xactions/`, `chi_xact_state/`, `chi_xact_base/` |
| Joint (System) | `CHI::Xact` | `chi/xact/chi_joint.hpp`, `chi_xact_global.hpp` |

---

## Multi-Issue Support

CHIron uses preprocessor flags to select the active CHI specification issue at compile time. All issue variants share the same namespace and type interfaces.

```mermaid
graph LR
    COMMON["Shared CHI\nInfrastructure"]

    B["CHI Issue B\nchi_b/"]
    C["CHI Issue C\nchi_c/"]
    E["CHI Issue E\nchi/ (default)"]
    EB["CHI Issue Eb\nchi_eb/"]

    subgraph flags["Compile-time flags"]
        FB["CHI_ISSUE_B_ENABLE"]
        FC["CHI_ISSUE_C_ENABLE"]
        FEB["CHI_ISSUE_EB_ENABLE"]
    end

    FB --> B
    FC --> C
    FEB --> EB

    B & C & E & EB --> COMMON
```

---

## Key Design Principles

### 1  Header-Only, Template-Parameterised

Every component is a C++20 template parameterised over a `FlitConfigurationConcept`. There are no linkage dependencies between library consumers; the entire library is included by copying headers.

```mermaid
graph LR
    CFG["FlitConfigurationConcept\n──────────────────\nNodeIDWidth  : 7–11 bits\nReqAddrWidth : 44–52 bits\nDataWidth    : 128/256/512 bits\nRSVDCWidth   : 0–32 bits"]
    FLIT["Flits::REQ&lt;config&gt;\nFlits::RSP&lt;config&gt;\nFlits::DAT&lt;config&gt;\nFlits::SNP&lt;config&gt;"]
    XACT["Xact::Xaction&lt;config&gt;"]
    JOINT["Xact::Joint&lt;config&gt;"]

    CFG --> FLIT --> XACT --> JOINT
```

### 2  Compile-Time State Tables

Transaction state-transition tables are generated with `constexpr` / `consteval` at compile time. This eliminates runtime table-build overhead and catches invalid transitions as compiler errors.

### 3  Event-Driven Denial Reporting

The `Joint` coordinator uses the `Gravity::EventBus` to publish denial events when a flit or sequence violates the CHI specification. Subscribers register callbacks and receive strongly-typed denial event objects.

### 4  Zero-Copy Connection Modes

`chi/basic/chi_conn.hpp` exposes two storage strategies for flit references, selectable per component level:

| Mode | Description |
|------|-------------|
| **Inline** | Flit data stored directly — zero heap allocation |
| **Pointer** | Shared pointer to external flit — zero copy |

### 5  Layered Separation of Concerns

Protocol bit-fields, channel wiring, transaction lifecycle, and system-level co-ordination are each handled in a separate, independently testable layer. Higher layers depend on lower layers through narrow, well-defined template interfaces.

---

## Related Guides

| Guide | Description |
|-------|-------------|
| [CHI Protocol Stack](chi-protocol-stack.md) | Nodes, channels, flits, interfaces, and configuration parameters |
| [Transaction Layer](transaction-layer.md) | Transaction types, the Xaction class, Joint, cache-state tracking |
| [CLog & Tools](clog-and-tools.md) | Transaction log formats and command-line analysis tools |
