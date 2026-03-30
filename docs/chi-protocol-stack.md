# CHI Protocol Stack

This guide covers the AMBA CHI protocol building blocks as implemented in CHIron: **node types**, **channels**, **flit structures**, **interfaces**, and **compile-time configuration parameters**.

---

## CHI Network Topology

An AMBA CHI interconnect connects three classes of agent through a shared network. CHIron models each agent type and each link.

```mermaid
graph TD
    subgraph RN["Requester Nodes (RN)"]
        RNF["RN-F\nFull coherent\n(cache)"]
        RND["RN-D\nDVM-capable\ncoherent"]
        RNI["RN-I\nNon-coherent\nrequester"]
    end

    subgraph HN["Home Nodes (HN)"]
        HNF["HN-F\nFull coherent\nhome"]
        HNI["HN-I\nNon-coherent\nhome"]
    end

    subgraph SN["Subordinate Nodes (SN)"]
        SNF["SN-F\nFull coherent\nsubordinate"]
        SNI["SN-I\nNon-coherent\nsubordinate"]
    end

    MN["MN\nMonitor Node"]

    RNF & RND & RNI <-->|"CHI Link"| HNF
    RNF & RND & RNI <-->|"CHI Link"| HNI
    HNF <-->|"CHI Link"| SNF & SNI
    MN -.->|"Observes"| HNF
```

### Node Type Summary

| Symbol | Full Name | Role |
|--------|-----------|------|
| **RN-F** | Requester Node — Full Coherent | CPU core caches, GPU L1/L2; issues all transaction types; holds cached data |
| **RN-D** | Requester Node — DVM Capable | Coherent requester that also handles Distributed Virtual Memory operations |
| **RN-I** | Requester Node — I/O | DMA engines, PCIe controllers; non-coherent read/write only |
| **HN-F** | Home Node — Full Coherent | Coherence point; arbitrates snoop traffic; interfaces with memory |
| **HN-I** | Home Node — I/O | Routes non-coherent traffic to SN-I |
| **SN-F** | Subordinate Node — Full Coherent | DRAM controller with coherent memory attributes |
| **SN-I** | Subordinate Node — I/O | Peripheral memory, MMIO regions |
| **MN** | Monitor Node | Passive observer; receives a copy of all transactions for debug/trace |

---

## CHI Link and Channels

Every CHI link is a pair of **TX** (transmit) / **RX** (receive) paths, each carrying one of four channel types.

```mermaid
graph LR
    subgraph RN_SIDE["Requester Node Side"]
        TXREQ_RN["TXREQ"]
        TXDAT_RN["TXDAT"]
        RXRSP_RN["RXRSP"]
        RXDAT_RN["RXDAT"]
        RXSNP_RN["RXSNP"]
    end

    subgraph HN_SIDE["Home Node Side"]
        RXREQ_HN["RXREQ"]
        RXDAT_HN["RXDAT"]
        TXRSP_HN["TXRSP"]
        TXDAT_HN["TXDAT"]
        TXSNP_HN["TXSNP"]
    end

    TXREQ_RN -- "REQ flits" --> RXREQ_HN
    TXDAT_RN -- "DAT flits" --> RXDAT_HN
    RXRSP_RN <-- "RSP flits" -- TXRSP_HN
    RXDAT_RN <-- "DAT flits" -- TXDAT_HN
    RXSNP_RN <-- "SNP flits" -- TXSNP_HN
```

### Channel Functions

| Channel | Direction (RN→HN) | Flit Type | Purpose |
|---------|-------------------|-----------|---------|
| **REQ** | RN → HN | `Flits::REQ` | Request initiation (read, write, atomic, dataless, …) |
| **RSP** | HN → RN | `Flits::RSP` | Completion responses, acknowledgements, credits |
| **DAT** | Bidirectional | `Flits::DAT` | Data payloads (CompData, CopyBack, SnpResp with data) |
| **SNP** | HN → RN | `Flits::SNP` | Snoop requests (SnpOnce, SnpUnique, SnpClean, …) |

---

## Flit Types and Fields

CHIron defines four concrete flit classes, all templated on a `FlitConfigurationConcept`.

```mermaid
classDiagram
    class FlitConfigurationConcept {
        +NodeIDWidth  : uint  %% 7–11 bits
        +ReqAddrWidth : uint  %% 44–52 bits
        +DataWidth    : uint  %% 128 / 256 / 512 bits
        +RSVDCWidth   : uint  %% 0 / 4 / 8 … 32 bits
        +EnableDataCheck : bool
        +EnablePoison    : bool
        +EnableMPAM      : bool
    }

    class REQ~config~ {
        +TgtID, SrcID, TxnID
        +Opcode : REQOpcode
        +Addr   : ReqAddrWidth bits
        +NS, Order, Excl, AllowRetry
        +PCrdType, MemAttr, SnpAttr
        +LPID, PrefetchTgtHint
    }

    class RSP~config~ {
        +TgtID, SrcID, TxnID
        +Opcode : RSPOpcode
        +RespErr, Resp
        +DBID, PCrdType
        +TraceTag
    }

    class DAT~config~ {
        +TgtID, SrcID, TxnID, HomeNID
        +Opcode : DATOpcode
        +RespErr, Resp
        +DBID, DataID, BE
        +Data   : DataWidth bits
        +DataCheck, Poison
    }

    class SNP~config~ {
        +SrcID, TxnID, FwdNID, FwdTxnID
        +Opcode : SNPOpcode
        +Addr   : ReqAddrWidth-3 bits
        +NS, DoNotGoToSD, RetToSrc
    }

    FlitConfigurationConcept --> REQ~config~
    FlitConfigurationConcept --> RSP~config~
    FlitConfigurationConcept --> DAT~config~
    FlitConfigurationConcept --> SNP~config~
```

---

## Interface Definitions

CHIron wraps channel sets into strongly-typed **interface** objects that represent an entire side of a CHI link.

```mermaid
graph LR
    subgraph RN_INTERFACES["RN Interfaces\n(chi/spec/chi_link_interfaces.hpp)"]
        RNF_IF["RN::F\n· TXREQ · TXDAT\n· RXRSP · RXDAT · RXSNP"]
        RND_IF["RN::D\n· TXREQ · TXDAT\n· RXRSP · RXDAT · RXSNP\n· RXDVM"]
        RNI_IF["RN::I\n· TXREQ · TXDAT\n· RXRSP · RXDAT"]
    end

    subgraph HN_INTERFACES["HN Interfaces"]
        HNF_IF["HN::F\n· RXREQ · RXDAT\n· TXRSP · TXDAT · TXSNP"]
        HNI_IF["HN::I\n· RXREQ · RXDAT\n· TXRSP · TXDAT"]
    end

    subgraph SN_INTERFACES["SN Interfaces"]
        SNF_IF["SN::F\n· RXREQ · RXDAT\n· TXRSP · TXDAT"]
        SNI_IF["SN::I\n· RXREQ · RXDAT\n· TXRSP · TXDAT"]
    end

    RNF_IF <-->|Link| HNF_IF
    RND_IF <-->|Link| HNF_IF
    RNI_IF <-->|Link| HNI_IF
    HNF_IF <-->|Link| SNF_IF
    HNI_IF <-->|Link| SNI_IF
```

---

## Configuration Parameters

All CHIron components are parameterised by a `FlitConfigurationConcept`. This allows a single codebase to cover every permitted CHI topology configuration.

```mermaid
graph TD
    subgraph cfg["FlitConfiguration&lt;…&gt;"]
        NID["NodeID Width\n7 · 8 · 9 · 10 · 11 bits\ndefault: 7"]
        ADDR["ReqAddr Width\n44 · 48 · 52 bits\ndefault: 48"]
        DATA["Data Width\n128 · 256 · 512 bits\ndefault: 256"]
        RSVDC["RSVDC Width\n0 · 4 · 8 · 12 · 16 · 24 · 32 bits\ndefault: 0"]
        OPT["Optional Features\nDataCheck · Poison · MPAM"]
    end

    cfg --> FLITS["Flit types\nREQ · RSP · DAT · SNP"]
    cfg --> XACT["Transaction types\nXaction&lt;config&gt;"]
    cfg --> JOINT["System coordinator\nJoint&lt;config&gt;"]
```

### Override via Preprocessor

```cpp
// Example: 9-bit NodeID, 48-bit address, 512-bit data bus
#define CHI_NODEID_WIDTH    9
#define CHI_REQ_ADDR_WIDTH  48
#define CHI_DATA_WIDTH      512
#define CHI_ISSUE_EB_ENABLE          // select Issue Eb
#include "chi/chi.hpp"
```

---

## Protocol Encoding

Opcode tables are generated at compile time from `chi/spec/chi_protocol_encoding.hpp` (≈ 3 200 lines). Each flit opcode is a strongly-typed enumeration; the encoding file maps every opcode to its decimal value, permitted field combinations, and specification-mandated constraints.

```mermaid
graph LR
    ENC["chi_protocol_encoding.hpp\nREQOpcode / RSPOpcode\nDATOpcode / SNPOpcode\n\nField constraints\nValid-bit tables"]
    FLIT["chi_protocol_flits.hpp\nBit-accurate struct layout\nField accessor methods"]
    UTIL["chi_util_decoding.hpp\nOpcode interpretation\nHuman-readable names"]
    EXPR["chi_expresso_flit.hpp\nFormatted flit printing\nDenial reason strings"]

    ENC --> FLIT --> UTIL --> EXPR
```

---

## Related Guides

| Guide | Description |
|-------|-------------|
| [Architecture Overview](architecture-overview.md) | Repository layout, layer model, design principles |
| [Transaction Layer](transaction-layer.md) | Transaction types, Xaction class, Joint coordinator |
| [CLog & Tools](clog-and-tools.md) | Transaction log formats and CLI analysis tools |
