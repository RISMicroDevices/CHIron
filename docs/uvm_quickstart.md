# CHIron UVM Quick-Start Guide

Get a CHI RN testbench running in five minutes.

## 1. Clone CHIron

```bash
git clone https://github.com/RISMicroDevices/CHIron.git
cd CHIron
```

## 2. Build the DPI-C Reference Model

```bash
g++ -std=c++20 -fPIC -shared \
    -I. \
    -DCHI_ISSUE_EB_ENABLE \
    -o chi_refmodel.so \
    uvm/chi_refmodel_dpi.cpp
```

## 3. Add Your DUT

Open `uvm/chi_top.sv` and un-comment the DUT block, connecting your RTL to the `chi_bus` interface signals.  For an initial smoke test without a DUT, leave the block commented out — the testbench will exercise the CHIron reference model alone.

## 4. Run a Test

### VCS

```bash
vcs -full64 -sverilog -ntb_opts uvm-1.2 \
    +incdir+uvm \
    uvm/chi_top.sv \
    -sv_lib chi_refmodel \
    -o simv

./simv +UVM_TESTNAME=chi_rand_test +NUM_TRANSACTIONS=50
```

### Xcelium

```bash
xrun -sv -uvm +incdir+uvm uvm/chi_top.sv \
    -sv_lib chi_refmodel.so \
    +UVM_TESTNAME=chi_rand_test
```

### Questa

```bash
vlog -sv +incdir+uvm uvm/chi_top.sv
vsim -sv_lib chi_refmodel chi_top +UVM_TESTNAME=chi_rand_test
```

## 5. Inspect the CLog

The monitor writes `chi_sim.clog` by default.  Decode it with:

```bash
./app/clog2log/clog2log chi_sim.clog
```

Example output:

```
[0042] RN-F#1  TXREQ  ReadShared  addr=0x0000_0040 txnid=0x001 size=64B
[0043] HN-F#8  RXRSP  CompData    txnid=0x001 resp=SC
[0044] RN-F#1  TXRSP  CompAck     txnid=0x001
```

Generate coverage metrics:

```bash
./app/clog2coverage/clog2coverage chi_sim.clog coverage.txt
cat coverage.txt
```

---

## Available Tests

| Test | Command-line | Description |
|---|---|---|
| `chi_read_test` | `+UVM_TESTNAME=chi_read_test` | 10 × ReadShared transactions |
| `chi_write_test` | `+UVM_TESTNAME=chi_write_test` | 10 × WriteUniqueFull transactions |
| `chi_rand_test` | `+UVM_TESTNAME=chi_rand_test +NUM_TRANSACTIONS=100` | 100 randomised transactions |

---

## Quick Configuration

All parameters are in `uvm/chi_pkg.sv`:

```systemverilog
parameter int unsigned NODEID_W = 7;   // 7–11
parameter int unsigned ADDR_W   = 48;  // 44–52
parameter int unsigned DATA_W   = 256; // 128/256/512
parameter int unsigned RSVDC_W  = 4;   // 0/4/8/12/16/24/32
```

Match these to the `FlitConfiguration<>` used by your DUT and in `uvm/chi_refmodel_dpi.cpp`.

---

## Writing Your First Sequence

```systemverilog
// In your test's run_phase:
chi_write_unique_seq seq = chi_write_unique_seq::type_id::create("seq");
seq.src_id     = 7'd1;
seq.tgt_id     = 7'd8;
seq.addr       = 48'h0000_0000_0040;
seq.write_data = 256'hDEAD_BEEF;
seq.txn_id     = 12'd0;
seq.start(env.agent.sequencer);
```

For complete documentation see [docs/uvm_integration_guide.md](uvm_integration_guide.md).
