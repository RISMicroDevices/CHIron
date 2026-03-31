# SystemVerilog Integration Guide

This guide explains how to compile the CHIron VIP DPI-C library and connect it to a
Verilog/SystemVerilog simulation environment.

---

## Directory Layout

```
CHIron/
  vip/
    chi_xact_vip_dpi.hpp    C++ DPI-C header
    chi_xact_vip_dpi.cpp    C++ DPI-C implementation
    chi_xact_vip_dpi.svh    SystemVerilog DPI import package
    chi_xact_monitor.sv     Example SV monitor module
  docs/vip/
    overview.md
    transaction_models.md
    state_trackers.md
    integration_sv.md       (this file)
```

---

## Step 1 — Compile the DPI-C Shared Library

### Prerequisites

- C++20-capable compiler (GCC 11+, Clang 13+)
- Simulator DPI-C headers (e.g., `svdpi.h` from VCS / Xcelium / Questa / Verilator)

### Build Command (GCC example)

```bash
g++ -std=c++20 -O2 -shared -fPIC \
    -I<CHIRON_ROOT> \
    -I<SIMULATOR_DPI_INCLUDE> \
    -DCHI_NODEID_WIDTH=7      \
    -DCHI_REQ_ADDR_WIDTH=48   \
    -DCHI_DATA_WIDTH=256       \
    -o libchironvip.so         \
    <CHIRON_ROOT>/vip/chi_xact_vip_dpi.cpp
```

Replace `<CHIRON_ROOT>` with the path to this repository and
`<SIMULATOR_DPI_INCLUDE>` with the directory containing `svdpi.h`.

### Flit-Width Overrides

All width parameters have sensible defaults but can be changed at compile time:

```bash
# 9-bit Node IDs, 512-bit data, MPAM enabled
g++ ... -DCHI_NODEID_WIDTH=9 -DCHI_DATA_WIDTH=512 -DCHI_MPAM_PRESENT=1 ...
```

---

## Step 2 — Import the DPI Package in SystemVerilog

Add the following include (or compile `chi_xact_vip_dpi.svh` as part of your package)
to every module that needs CHIron services:

```systemverilog
`include "chi_xact_vip_dpi.svh"
```

This declares all `import "DPI-C"` functions and defines helper constants:

```systemverilog
`CHIRONVIP_NODE_TYPE_RN_F   // 1
`CHIRONVIP_NODE_TYPE_HN_F   // 5
`CHIRONVIP_NODE_TYPE_SN_F   // 9
// ...
`CHIRONVIP_CACHE_STATE_UC   // bitmask for UC state
`CHIRONVIP_CACHE_STATE_I    // bitmask for I (invalid)
// ...
```

---

## Step 3 — Simulator-Specific Load

### Synopsys VCS

```bash
vcs -sverilog +acc \
    -sv_lib libchironvip   \
    -f filelist.f
```

### Cadence Xcelium

```bash
xrun -sv \
     -sv_lib libchironvip  \
     -f filelist.f
```

### Mentor / Siemens Questa

```bash
vlog -sv chi_xact_vip_dpi.svh chi_xact_monitor.sv dut.sv
vsim -sv_lib libchironvip work.tb_top
```

### Verilator

Verilator compiles the shared library together with the simulation:

```bash
verilator --cc --exe -CFLAGS "-std=c++20" \
    --Mdir obj_dir                         \
    -LDFLAGS "-L. -lchironvip"            \
    chi_xact_monitor.sv dut.sv tb_top.sv
make -C obj_dir -f Vtb_top.mk
```

---

## Step 4 — Wire the Monitor

Instantiate `chi_xact_monitor` alongside the interface port of the node you are
monitoring.  The monitor is fully passive — it only observes flits and reports
violations.

```systemverilog
chi_xact_monitor #(
    .NODE_ID     (7'h04),       // Node ID of the RN-F being monitored
    .HN_NODE_ID  (7'h40),       // Home node ID
    .NODE_TYPE   (`CHIRONVIP_NODE_TYPE_RN_F)
) u_rnf_monitor (
    .clk          (clk),
    // TXREQ channel
    .txreq_valid  (rnf0_txreq_valid),
    .txreq_flit   (rnf0_txreq_flit),
    // RXRSP channel
    .rxrsp_valid  (rnf0_rxrsp_valid),
    .rxrsp_flit   (rnf0_rxrsp_flit),
    // RXDAT channel
    .rxdat_valid  (rnf0_rxdat_valid),
    .rxdat_flit   (rnf0_rxdat_flit),
    // RXSNP channel
    .rxsnp_valid  (rnf0_rxsnp_valid),
    .rxsnp_flit   (rnf0_rxsnp_flit),
    // TXRSP channel
    .txrsp_valid  (rnf0_txrsp_valid),
    .txrsp_flit   (rnf0_txrsp_flit),
    // TXDAT channel
    .txdat_valid  (rnf0_txdat_valid),
    .txdat_flit   (rnf0_txdat_flit)
);
```

---

## Step 5 — Check Denial Codes

Every DPI call returns an `int` denial code.  Zero (`CHIRONVIP_ACCEPTED`) means the
flit was accepted; any other value indicates a protocol violation:

```systemverilog
int denial;
denial = CHIronVIP_RNF_NextTXREQ(rnf_joint, global_ctx, $time,
                                  txreq_flit, REQ_FLIT_BITS);
if (denial != `CHIRONVIP_ACCEPTED) begin
    $error("[CHIron] TXREQ protocol violation, code=%0d", denial);
    // Optionally query the name string via CHIronVIP_GetDenialName()
end
```

---

## Step 6 — Cache State Checking

For RN-F nodes, attach a state map and check each flit against it:

```systemverilog
chandle state_map;

initial begin
    state_map = CHIronVIP_CreateRNCacheStateMap();
    CHIronVIP_StateMap_SetSilentEviction(state_map, 1);
end

// On each TXREQ fire:
always @(posedge clk) begin
    if (txreq_valid) begin
        int d;
        d = CHIronVIP_StateMap_NextTXREQ(state_map, txreq_flit[47:0],
                                          txreq_flit, REQ_FLIT_BITS);
        assert (d == `CHIRONVIP_ACCEPTED)
            else $error("[CHIron] Cache state violation on TXREQ");
    end
end
```

---

## Common Integration Patterns

### Pattern A: Passive Protocol Monitor

Use only the Joint (no state map).  Suitable for any node type.

```
VIP role → observe flits → report violations via denial codes
```

### Pattern B: Protocol + Coherence Monitor (RN-F)

Use the Joint AND the state map together.

```
VIP role → observe flits → protocol check (joint) + coherence check (state map)
```

### Pattern C: Scoreboard

Subscribe to `OnComplete` events to cross-check transaction outcomes against a
reference model:

```cpp
// C++ side (inside DPI callback registration)
rnJoint.OnComplete += [](const Xact::JointXactionCompleteEvent<config>& ev) {
    // Compare ev.xaction result with expected value
};
```

---

## Flit-Width Reference

The monitor/DPI wrapper uses `bit[511:0]` to pass all flit types to C++.
The actual number of significant bits depends on the configuration:

| Flit | Typical width (7-bit NodeID, 48-bit addr, 256-bit data) |
|------|---------------------------------------------------------|
| REQ  | 97 bits |
| SNP  | 88 bits |
| RSP  | 45 bits |
| DAT  | ~394 bits |

Pass the actual bit width as the `flitLen` argument to every DPI call.  The convenience
constants `CHIRONVIP_REQ_FLIT_BITS`, `CHIRONVIP_RSP_FLIT_BITS`, etc. are provided by
`chi_xact_vip_dpi.svh` for the compiled-in configuration.

---

## Troubleshooting

| Symptom | Likely cause |
|---------|-------------|
| All flits denied with `XACT_DENIED_CHANNEL_*` | Node ID or `nodeType` mismatch in topology |
| `XACT_DENIED_TXNID_NOT_EXIST` on responses | Response received before corresponding request |
| State map always returns `ACCEPTED` | Silent-transition flags not enabled correctly |
| Linker errors for `CHIronVIP_*` symbols | `libchironvip.so` not loaded; check `-sv_lib` flag |
| Compile errors about C++20 concepts | Use GCC 11+ or Clang 13+; add `-std=c++20` |
