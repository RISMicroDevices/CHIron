# CHIron UVM Integration Guide

## Overview

[CHIron](https://github.com/RISMicroDevices/CHIron) is the world's first open-source AMBA CHI infrastructure library written in modern C++20. This guide explains how to use CHIron as the **verification kernel** of a SystemVerilog UVM testbench — combining CHIron's protocol intelligence (flit definitions, opcode tables, transaction state machines, and CLog logging) with UVM's test management infrastructure.

### What "kernel" means

CHIron provides the authoritative **protocol layer**:

| CHIron Capability | UVM Role |
|---|---|
| Flit field definitions (`chi/spec/chi_protocol_flits.hpp`) | Defines signal widths used in `chi_if.sv` and `chi_pkg.sv` |
| Opcode tables (`chi/spec/chi_protocol_encoding.hpp`) | Populates opcode `parameter` constants in `chi_pkg.sv` |
| Transaction state machines (`chi/xact/`) | Powers the `chi_scoreboard.sv` reference model via DPI-C |
| CLog binary logger (`clog/clog_b/`, `clog/clog_t/`) | Called by `chi_monitor.sv` to capture every flit |
| Post-processing tools (`app/`) | Used offline to analyse coverage and decode logs |

UVM provides the test infrastructure: sequencer, driver, monitor, scoreboard framework, coverage, and `run_test()` phasing.

---

## Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│                          UVM Testbench                                   │
│                                                                          │
│  ┌──────────┐   ┌────────────────────────────────────────────────────┐  │
│  │   Test   │   │  chi_env                                           │  │
│  │  (SVh)   │   │  ┌────────────────────┐                            │  │
│  └────┬─────┘   │  │   chi_agent        │                            │  │
│       │         │  │  ┌──────────────┐  │  analysis_port             │  │
│       │         │  │  │  sequencer   │  ├──────────────────┐         │  │
│       │         │  │  ├──────────────┤  │                  │         │  │
│       └─────────┼──►  │   driver     │  │     ┌────────────▼──────┐  │  │
│                 │  │  ├──────────────┤  │     │ chi_scoreboard    │  │  │
│                 │  │  │   monitor    │──┼──►  │ (CHIron refmodel  │  │  │
│                 │  │  └──────────────┘  │     │  via DPI-C)       │  │  │
│                 │  └──────────┬─────────┘     └───────────────────┘  │  │
│                 │             │                ┌───────────────────┐  │  │
│                 │             │                │  chi_coverage     │  │  │
│                 │             │                └───────────────────┘  │  │
│                 └─────────────┼────────────────────────────────────┘  │  │
└───────────────────────────────│────────────────────────────────────────┘  │
                                │ SV Interface (chi_if)                      │
                    ┌───────────▼──────────────┐                            │
                    │    DUT  (CHI RN / HN)    │                            │
                    └──────────────────────────┘                            │

                    ┌────────────────────────────────────────────────────┐  │
                    │  CHIron C++ Kernel  (DPI-C shared object)          │  │
                    │  chi_refmodel_dpi.cpp wraps:                       │  │
                    │   • FlitConfiguration<7,48,4,4,256,false,false>   │  │
                    │   • Xact::Router / XactionAllocatingRead, ...      │  │
                    │   • CLogB_WriteRecord (binary trace)               │  │
                    └────────────────────────────────────────────────────┘
```

---

## Repository Structure

After following this guide your repository tree will look like:

```
CHIron/
├── chi/                  ← CHIron protocol layer (C++ headers)
│   ├── basic/
│   ├── spec/             ← Flit definitions, opcodes
│   ├── util/
│   └── xact/             ← Transaction state machines
├── clog/
│   ├── clog_b/           ← Binary CLog DPI-C interface (.svh + C++)
│   └── clog_t/           ← Text CLog DPI-C interface (.svh + C++)
├── app/                  ← Post-processing tools
├── uvm/                  ← UVM testbench (this guide)
│   ├── chi_pkg.sv        ← SV package: constants, opcodes, flit structs
│   ├── chi_if.sv         ← CHI bus interface
│   ├── chi_seq_item.sv   ← UVM sequence item
│   ├── chi_driver.sv     ← UVM RN driver
│   ├── chi_monitor.sv    ← UVM monitor + CLog integration
│   ├── chi_agent.sv      ← UVM agent (driver + monitor + sequencer)
│   ├── chi_scoreboard.sv ← Protocol checker (CHIron reference model)
│   ├── chi_coverage.sv   ← Functional coverage
│   ├── chi_seq_lib.sv    ← Sequence library
│   ├── chi_env.sv        ← UVM environment
│   ├── chi_test.sv       ← Example tests
│   ├── chi_top.sv        ← Top-level testbench module
│   └── chi_refmodel_dpi.cpp ← C++ DPI-C shim for CHIron reference model
└── docs/
    ├── uvm_integration_guide.md  ← This file
    └── uvm_quickstart.md         ← Quick-start instructions
```

---

## Prerequisites

| Tool | Minimum version |
|---|---|
| C++ compiler | GCC 11 / Clang 13 (C++20 required) |
| SystemVerilog simulator | Synopsys VCS 2022.06, Cadence Xcelium 23.03, Mentor Questa 2022.4, or equivalent |
| CMake | 3.16 (for CHIron C++ tests only) |
| UVM | 1.2 or IEEE 1800.2 |

---

## Step 1 — Configure `chi_pkg.sv` for Your DUT

Open `uvm/chi_pkg.sv` and set the four parameters to match your DUT's CHI `FlitConfiguration<>` template instantiation:

```systemverilog
parameter int unsigned NODEID_W = 7;   // CHI NodeID_Width  (7–11)
parameter int unsigned ADDR_W   = 48;  // CHI Req_Addr_Width (44–52)
parameter int unsigned DATA_W   = 256; // CHI Data_Width (128/256/512)
parameter int unsigned RSVDC_W  = 4;   // CHI RSVDC_Width (0/4/8/12/16/24/32)
```

These values must match the C++ `FlitConfiguration<>` used in `chi_refmodel_dpi.cpp`.

---

## Step 2 — Build the CHIron DPI-C Shared Object

The `chi_refmodel_dpi.cpp` shim wraps CHIron's C++ transaction state machines.

### 2.1 Using GCC

```bash
g++ -std=c++20 -fPIC -shared \
    -I<chiron_root> \
    -DCHI_ISSUE_EB_ENABLE \
    -o chi_refmodel.so \
    <chiron_root>/uvm/chi_refmodel_dpi.cpp
```

Replace `<chiron_root>` with the absolute path to the CHIron repository.

### 2.2 Extending the Reference Model

The provided `chi_refmodel_dpi.cpp` is a **stub** that validates flit lengths and returns no errors.  To enable full protocol checking, extend the `RefModel::feed()` function to:

1. **Unpack** the raw bytes into a typed `CHI::Flits::REQ<Config>` flit using CHIron's flit struct bit-field accessors.
2. **Route** the flit to the appropriate `Xaction` state machine via `CHI::Xact::Router<Config>`.
3. **Advance** the transaction state by calling `xact->NextRSP()` / `NextDAT()` on response/data flits.
4. **Check** for denial codes returned by `NextRSP()` / `NextDAT()` — a non-`Accepted` code indicates a protocol violation.

Key CHIron types to use:

```cpp
// Typed flit (example for REQ channel)
CHI::Flits::REQ<Config> req_flit;
req_flit.SetOpcode(CHI::Opcodes::REQ::ReadShared);
req_flit.SetAddr(address);
req_flit.SetSrcID(src_id);
req_flit.SetTxnID(txn_id);
req_flit.SetSize(6);  // 64-byte cache line

// Transaction state machine for an allocating read
auto xact = std::make_shared<CHI::Xact::XactionAllocatingRead<Config>>(
    global_context, fired_req_flit);

// Advance on RSP flit
CHI::Xact::XactDenialEnum denial = xact->NextRSP(global_ctx, fired_rsp_flit);
if (denial != CHI::Xact::XactDenial::Accepted) {
    // Protocol error detected
}
```

Refer to the CHIron transaction tests in `test/tc_chi_xact_state/` for working examples of how the state machines are driven.

---

## Step 3 — Connect `chi_if` to the DUT

Edit `uvm/chi_top.sv` and un-comment the DUT instantiation block, replacing `chi_rn_dut` with your DUT module name and adjusting the port map.

```systemverilog
chi_rn_dut dut (
    .clk               (clk),
    .rst_n             (rst_n),
    // TXREQ (DUT drives these)
    .txreq_flitv       (chi_bus.txreq_flitv),
    .txreq_opcode      (chi_bus.txreq_opcode),
    ...
    // RXRSP (testbench drives these)
    .rxrsp_flitv       (chi_bus.rxrsp_flitv),
    .rxrsp_opcode      (chi_bus.rxrsp_opcode),
    ...
);
```

### Signal naming conventions

The `chi_if.sv` interface uses the naming convention from the CHI specification:

| Prefix | Description |
|---|---|
| `txreq_*` | TX Request channel (RN → HN) |
| `txrsp_*` | TX Response channel (RN → HN) |
| `txdat_*` | TX Data channel (RN → HN) |
| `rxrsp_*` | RX Response channel (HN → RN) |
| `rxdat_*` | RX Data channel (HN → RN) |
| `rxsnp_*` | RX Snoop channel (HN → RN) |
| `*_flitv` | Flit valid |
| `*_flitpend` | Flit pending (one cycle before flitv) |
| `*_lcrdv` | Link credit valid |

---

## Step 4 — Configure CLog Logging

The monitor automatically writes a binary CLog file that can be post-processed by CHIron's app tools.  Configure via `uvm_config_db` in your test:

```systemverilog
// In test::build_phase():
uvm_config_db #(bit)   ::set(this, "env.agent.monitor", "log_enable", 1);
uvm_config_db #(string)::set(this, "env.agent.monitor", "log_path",   "sim.clog");
uvm_config_db #(int)   ::set(this, "env.agent.monitor", "nodeid",     1);
```

### Multi-node logging

For multi-node simulations (multiple RN/HN/SN agents), use the **shared handle** API from `clogdpi_b.svh` so all monitors write to the same log file:

```systemverilog
// In the top-level setup (before monitors start):
chandle h;
int status;
h = CLogB_OpenFile("system.clog", status);
CLogB_ShareHandle("main_log", h);

// In each monitor's start_of_simulation_phase:
CLogB_SharedWriteRecord("main_log", cycle, nodeid, channel, flit, flit_len);
```

### Post-processing the CLog

After simulation, use CHIron's app tools to analyse the log:

```bash
# Decode binary log to human-readable text
./app/clog2log/clog2log sim.clog

# Generate coverage metrics
./app/clog2coverage/clog2coverage sim.clog coverage.txt

# Report individual flit details
./app/report_flit/report_flit sim.clog
```

---

## Step 5 — Write Sequences

### Built-in sequences (`chi_seq_lib.sv`)

| Sequence | Description |
|---|---|
| `chi_read_shared_seq` | ReadShared + CompAck |
| `chi_read_unique_seq` | ReadUnique + CompAck |
| `chi_write_unique_seq` | WriteUniqueFull + NonCopyBackWrData |
| `chi_write_back_seq` | WriteBackFull + CopyBackWrData |
| `chi_atomic_seq` | AtomicSwap + CompAck |
| `chi_snp_resp_seq` | SnpResp (invalidate) |
| `chi_rand_seq` | Randomised mix |

### Custom sequence example

```systemverilog
class my_write_no_snp_seq extends chi_base_seq;
  `uvm_object_utils(my_write_no_snp_seq)

  rand logic [ADDR_W-1:0] addr;
  rand logic [DATA_W-1:0] data;

  task body();
    chi_seq_item req = make_req(REQ_WriteNoSnpFull, addr);
    start_item(req);
    finish_item(req);

    // Wait for DBIDResp before sending data:
    // In a reactive HN model, the HN drives rxrsp_flitv with DBIDResp;
    // the sequence waits on a response mailbox populated by the monitor.
    chi_seq_item wdat = make_write_data(
        DAT_NonCopyBackWrData[3:0], tgt_id, txn_id, data);
    start_item(wdat);
    finish_item(wdat);
  endtask
endclass
```

---

## Step 6 — Run the Simulation

### VCS

```bash
vcs -full64 -sverilog -ntb_opts uvm-1.2 \
    +incdir+<chiron_root>/uvm \
    +incdir+<uvm_home>/sv \
    <chiron_root>/uvm/chi_top.sv \
    -sv_lib chi_refmodel \
    -o simv

./simv +UVM_TESTNAME=chi_rand_test +NUM_TRANSACTIONS=100
```

### Xcelium

```bash
xrun -sv -uvm \
    +incdir+<chiron_root>/uvm \
    <chiron_root>/uvm/chi_top.sv \
    -sv_lib chi_refmodel.so \
    +UVM_TESTNAME=chi_rand_test
```

### Questa

```bash
vlog -sv +incdir+<chiron_root>/uvm \
     <chiron_root>/uvm/chi_top.sv
vsim -sv_lib chi_refmodel chi_top \
     +UVM_TESTNAME=chi_rand_test
```

---

## CHI Issue Support

| Issue | FlitConfiguration define | Notes |
|---|---|---|
| Issue E (default) | `CHI_ISSUE_EB_ENABLE` with MPAM=false | Full transaction support |
| Issue EB | `CHI_ISSUE_EB_ENABLE` with MPAM=true | MakeReadUnique, StashOnceSep, combined writes |
| Issue B | `CHI_ISSUE_B_ENABLE` | Basic support; transaction modeling not yet complete |

To change the issue, update the `#define` in `chi_refmodel_dpi.cpp` and the `CLOG_ISSUE_*` value written in `chi_monitor.sv`'s `CLogB_WriteParameters` call.

---

## Known Limitations

The following CHI features are not yet modelled in CHIron and therefore not checked by the scoreboard:

- **MTE** (Memory Tagging Extensions) — not on roadmap
- **DVM** (Distributed Virtual Memory) — not on roadmap
- **Exclusive Monitor tracking** — not implemented
- **SnpPreferUnique / SnpPreferUniqueFwd under exclusive sequence** — partial support
- **WriteDataCancel** — needs more detailed modeling

See [ERRATA.md](../ERRATA.md) for specification deviations made in CHIron.

---

## File Reference

| File | Purpose |
|---|---|
| `uvm/chi_pkg.sv` | SV package: configuration parameters, opcodes, flit structs, helper functions |
| `uvm/chi_if.sv` | Parameterised CHI bus interface with clocking blocks |
| `uvm/chi_seq_item.sv` | UVM sequence item: decoded flit fields + raw 512-bit flit for CLog |
| `uvm/chi_driver.sv` | UVM driver: credit-aware TX channel driving |
| `uvm/chi_monitor.sv` | UVM monitor: captures all 6 channels, calls CLog DPI-C |
| `uvm/chi_agent.sv` | UVM agent: bundles driver, monitor, sequencer |
| `uvm/chi_scoreboard.sv` | Protocol checker: forwards flits to CHIron reference model |
| `uvm/chi_coverage.sv` | Functional coverage: opcodes, cache states, QoS, cross-coverage |
| `uvm/chi_seq_lib.sv` | Built-in sequences: ReadShared, WriteUnique, WriteBack, Atomic, … |
| `uvm/chi_env.sv` | UVM environment: connects agent → scoreboard + coverage |
| `uvm/chi_test.sv` | Example tests: read, write, randomised |
| `uvm/chi_top.sv` | Top-level: DUT instantiation, virtual interface, `run_test()` |
| `uvm/chi_refmodel_dpi.cpp` | C++ DPI-C shim exposing CHIron to the scoreboard |
| `clog/clog_b/clogdpi_b.svh` | DPI-C declarations for binary CLog (included by monitor) |
| `clog/clog_t/clogdpi_t.svh` | DPI-C declarations for text CLog (optional alternative) |
