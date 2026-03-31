# CHIron as a Verification IP for Verilog/SystemVerilog

## Overview

CHIron provides C++ transaction models and cache-state trackers for the AMBA CHI protocol.
When used as a **Verification IP (VIP)**, CHIron sits alongside a Verilog or SystemVerilog
(SV) testbench and is integrated via the **SystemVerilog DPI-C** mechanism.

```
┌──────────────────────────────────────────────────┐
│               SystemVerilog Testbench             │
│                                                  │
│  ┌──────────────────┐  ┌───────────────────────┐ │
│  │  DUT (RTL)       │  │  SV Monitor / Checker │ │
│  │  RN-F / HN-F /   │←─│  chi_xact_monitor.sv  │ │
│  │  SN-F            │  │  chi_xact_checker.sv  │ │
│  └──────────────────┘  └──────────┬────────────┘ │
│                                   │ DPI-C calls  │
└───────────────────────────────────┼──────────────┘
                                    │
              ┌─────────────────────▼─────────────────────┐
              │         CHIron VIP DPI-C Shared Library    │
              │  chi_xact_vip_dpi.cpp  (compiled .so/.dll) │
              │                                            │
              │  ┌───────────────┐  ┌───────────────────┐ │
              │  │  RNFJoint     │  │  RNCacheStateMap  │ │
              │  │  SNFJoint     │  │  (state tracker)  │ │
              │  └───────────────┘  └───────────────────┘ │
              └────────────────────────────────────────────┘
```

## Core Components

| Component | C++ Class | Role in VIP |
|-----------|-----------|-------------|
| Transaction model (requester) | `CHI::Eb::Xact::RNFJoint<config>` | Tracks all in-flight transactions from an RN-F node |
| Transaction model (home node) | `CHI::Eb::Xact::SNFJoint<config>` | Tracks all in-flight transactions at an HN-F/SN-F |
| Cache state tracker | `CHI::Eb::Xact::RNCacheStateMap<config>` | Tracks per-address coherence state for an RN-F |
| Protocol context | `CHI::Eb::Xact::Global<config>` | Holds topology (node IDs/types) and checker settings |

## Integration Flow

1. **Compile** `vip/chi_xact_vip_dpi.cpp` into a shared library together with your simulator's
   DPI-C runtime.
2. **Import** `vip/chi_xact_vip_dpi.svh` into every SV package/module that uses CHIron.
3. **Instantiate** and wire the supplied `chi_xact_monitor.sv` alongside the DUT interface.
4. **React** to protocol violations through the return codes of DPI calls or via SV assertions.

## Supported CHI Issues

| Issue | Status |
|-------|--------|
| CHI Issue B | Basic support |
| CHI Issue C | Basic support |
| **CHI Issue E / E.b** | **Full transaction and state-tracking support** |

The DPI-C wrapper (`vip/chi_xact_vip_dpi.cpp`) targets **CHI Issue E.b** by default.
Compile-time macros can override the flit-width parameters:

| Macro | Default | Description |
|-------|---------|-------------|
| `CHI_NODEID_WIDTH` | `7` | Node-ID field width in bits |
| `CHI_REQ_ADDR_WIDTH` | `48` | Request address width in bits |
| `CHI_REQ_RSVDC_WIDTH` | `0` | RSVDC field width in REQ flits |
| `CHI_DAT_RSVDC_WIDTH` | `0` | RSVDC field width in DAT flits |
| `CHI_DATA_WIDTH` | `256` | Data payload width in bits |
| `CHI_DATACHECK_PRESENT` | `false` | Include DataCheck field |
| `CHI_POISON_PRESENT` | `false` | Include Poison field |
| `CHI_MPAM_PRESENT` | `false` | Include MPAM field |

## Files

```
vip/
  chi_xact_vip_dpi.hpp     C++ DPI-C header (extern "C" declarations)
  chi_xact_vip_dpi.cpp     C++ DPI-C implementation
  chi_xact_vip_dpi.svh     SystemVerilog DPI import declarations
  chi_xact_monitor.sv      Example SV monitor wrapping the DPI calls

docs/vip/
  overview.md              This file
  transaction_models.md    Guide: RNFJoint / SNFJoint
  state_trackers.md        Guide: RNCacheStateMap
  integration_sv.md        Step-by-step SV integration guide
```

## Next Steps

- [Transaction Models Guide](transaction_models.md)
- [Cache State Tracker Guide](state_trackers.md)
- [SystemVerilog Integration Guide](integration_sv.md)
