# CLog Format & Command-Line Tools

CHIron records CHI transactions in **CLog** (CHI Transaction Log), a structured format available in both binary (`CLog.B`) and text (`CLog.T`) variants. A suite of CLI tools converts between formats and generates coverage/configuration reports.

---

## CLog Overview

```mermaid
graph LR
    SIM["Simulator /\nVerification Environment"]
    CLOGB["CLog.B\n(binary, compressed)"]
    CLOGT["CLog.T\n(human-readable text)"]
    JOINT["Xact::Joint\n(transaction analysis)"]

    subgraph tools["Command-Line Tools (app/)"]
        X2C["xsdb2clog\nXiangShan debug → CLog"]
        C2L["clog2log\nCLog → human log"]
        C2COV["clog2coverage\nCLog → coverage report"]
        C2CFG["clog2mkcfg\nCLog → config file"]
        RPT["report_flit\nFlit reporting"]
    end

    SIM --> CLOGB
    SIM --> CLOGT
    CLOGB <-->|"clog2log"| CLOGT
    CLOGB --> tools
    CLOGT --> tools
    JOINT --> CLOGB
```

---

## CLog.B — Binary Format

**File:** `clog/clog_b/clog_b.hpp`, `clog_b_tag.hpp`  
**Namespace:** `CLog::CLogB`  
**Dependency:** zlib (optional compression)

### File Structure

A CLog.B file is a sequence of **tagged blocks**. Each block starts with a 1-byte tag type, followed by the block's payload. The file ends with an EOF tag.

```mermaid
graph TD
    FILE["CLog.B file"]

    subgraph tags["Tagged Blocks (sequential)"]
        T0["Tag 0x00 — CHI Parameters\n1 byte per field\nIssue · NodeIDWidth · ReqAddrWidth\nRSVDCWidths · DataWidth\nDataCheck · Poison · MPAM flags"]
        T1["Tag 0x01 — CHI Topologies\ncount : 2B\nnodes[] = { nodeType:1B, nodeId:2B }"]
        T2["Tag 0x10 — CHI Records\ncount : 4B  timeBase : 8B\nlength : 4B  compressed : 4B\nrecords[] = {\n  timeShift : 4B\n  nodeId    : 2B\n  channel   : 1B\n  flitLen   : 1B\n  flit[]    : flitLen B\n}"]
        TEOF["Tag 0x7F — EOF"]
    end

    FILE --> T0 --> T1 --> T2 --> TEOF
```

### Tag Type Encodings

| Tag byte | Name | Description |
|----------|------|-------------|
| `0x00` | `CHI_PARAMETERS` | Protocol configuration (issue, widths, optional features) |
| `0x01` | `CHI_TOPOS` | Node topology (NodeID → NodeType mapping) |
| `0x10` | `CHI_RECORDS` | Flit records with timestamps (optionally zlib-compressed) |
| `0x7F` | `_EOF` | End-of-file sentinel |

### Record Layout

Each flit record within a `CHI_RECORDS` block:

```
┌──────────────┬──────────┬─────────┬──────────┬────────────────┐
│ timeShift 4B │ nodeId 2B│chan   1B│flitLen 1B│ flit[flitLen]  │
└──────────────┴──────────┴─────────┴──────────┴────────────────┘
```

- **timeShift** — timestamp delta relative to `timeBase` (nanoseconds)
- **nodeId** — originating node (maps to topology)
- **channel** — `TXREQ=0, TXRSP=1, TXDAT=2, TXSNP=3, RXREQ=4, RXRSP=5, RXDAT=6, RXSNP=7`
- **flit** — raw flit bytes (length varies by `DataWidth`)

### SystemVerilog DPI Bindings

`clog/clog_b/clogdpi_b.hpp` and `clogdpi_b.svh` provide DPI-C functions for writing CLog.B records directly from a SystemVerilog simulation:

```systemverilog
import "DPI-C" function void clogdpi_b_open(string filename);
import "DPI-C" function void clogdpi_b_write_req(
    longint time, int nodeId, int channel, ...);
import "DPI-C" function void clogdpi_b_close();
```

---

## CLog.T — Text Format

**File:** `clog/clog_t/clog_t.hpp`, `clog_t_util.hpp`  
**Namespace:** `CLog::CLogT`

CLog.T is a human-readable, line-oriented format. Each line encodes one flit record with space-separated fields, making it easy to inspect with standard Unix tools (`grep`, `awk`, etc.).

```mermaid
graph LR
    subgraph line["Text Record Line"]
        TS["timestamp"]
        NID["nodeId"]
        CH["channel"]
        FLIT["flit hex fields…"]
    end

    TS --> NID --> CH --> FLIT
```

### SystemVerilog DPI Bindings

`clog/clog_t/clogdpi_t.hpp` and `clogdpi_t.svh` provide equivalent DPI-C functions for text-format logging:

```systemverilog
import "DPI-C" function void clogdpi_t_open(string filename);
import "DPI-C" function void clogdpi_t_write_flit(
    longint time, int nodeId, string channel, ...);
import "DPI-C" function void clogdpi_t_close();
```

---

## CLog Parameters

Both formats embed a `CLog::Parameters` object describing the exact CHI configuration used during capture. These values are required to correctly decode flit fields.

```mermaid
classDiagram
    class Parameters {
        +Issue issue              %% B or Eb
        +size_t nodeIdWidth       %% 7–11 bits
        +size_t reqAddrWidth      %% 44–52 bits
        +size_t reqRsvdcWidth     %% 0–32 bits
        +size_t datRsvdcWidth     %% 0–32 bits
        +size_t dataWidth         %% 128/256/512 bits
        +bool dataCheckPresent
        +bool poisonPresent
        +bool mpamPresent
    }

    class Issue {
        <<enumeration>>
        B  = 0
        Eb = 3
    }

    Parameters --> Issue
```

---

## Command-Line Tools

All tools are built from their respective `app/<tool>/` directories using the provided Makefile.

```mermaid
graph LR
    subgraph inputs["Inputs"]
        XSDB["XiangShan debug log\n(.xsdb)"]
        CLOGB2["CLog.B file"]
        FLIT["Raw flit hex\n(stdin or file)"]
    end

    subgraph outputs["Outputs"]
        LOG["Human-readable log\n(stdout / .log)"]
        COV["Coverage report\n(.csv / .json)"]
        CFG["Configuration file\n(.cfg)"]
        REPT["Flit report\n(stdout)"]
    end

    XSDB  -->|"xsdb2clog"| CLOGB2
    CLOGB2 -->|"clog2log"| LOG
    CLOGB2 -->|"clog2coverage"| COV
    CLOGB2 -->|"clog2mkcfg"| CFG
    FLIT  -->|"report_flit"| REPT
```

### Tool Summaries

| Tool | Source | Purpose |
|------|--------|---------|
| **xsdb2clog** | `app/xsdb2clog/` | Converts raw XiangShan XSDB debug output into a CLog.B file |
| **clog2log** | `app/clog2log/` | Decodes a CLog.B file and prints a human-readable transaction log |
| **clog2coverage** | `app/clog2coverage/` | Analyses a CLog.B file and generates a protocol coverage report |
| **clog2mkcfg** | `app/clog2mkcfg/` | Extracts the CHI parameter block from a CLog.B and emits a config file |
| **report_flit** | `app/report_flit/` | Decodes and pretty-prints a raw flit provided on stdin |

### Building the Tools

Each tool has its own `Makefile`. There is a shared `Makefile.config` with common compiler settings.

```bash
# Build clog2log for Issue Eb
cd app/clog2log
make

# Run
./clog2log input.clog > transactions.log
```

---

## CLog Reader API

For programmatic access to CLog.B files, use the callback-based reader in `CLog::CLogB::ReaderWithCallback`:

```cpp
#include "clog/clog_b/clog_b.hpp"

CLog::CLogB::ReaderWithCallback reader;

reader.callbackOnTagCHIParameters = [](const CLog::CLogB::TagCHIParameters& tag) {
    // inspect tag.parameters
};

reader.callbackOnTagCHITopologies = [](const CLog::CLogB::TagCHITopologies& tag) {
    // inspect tag.nodes
};

reader.callbackOnTagCHIRecords = [](const CLog::CLogB::TagCHIRecords& tag) {
    for (auto& rec : tag.records) {
        // rec.timeShift, rec.nodeId, rec.channel, rec.flit
    }
};

std::ifstream f("trace.clog", std::ios::binary);
reader.Read(f);
```

---

## Related Guides

| Guide | Description |
|-------|-------------|
| [Architecture Overview](architecture-overview.md) | Repository layout, layer model, design principles |
| [CHI Protocol Stack](chi-protocol-stack.md) | Nodes, channels, flits, interfaces, configuration parameters |
| [Transaction Layer](transaction-layer.md) | Transaction types, Xaction class, Joint coordinator |
