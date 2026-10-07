# cohestra_vcs

VCS/DPI harness for a CCHI Type-1 home — the sibling of [`cohestra_v3`](../cohestra_v3/README.md) (Verilator flow). It runs the **same harness logic** (`executable.cpp` here is a clone of `cohestra_v3/executable.cpp`), but the DUT is simulated by **Synopsys VCS** and reached over **SystemVerilog DPI** instead of a Verilated model.

The first DUT target is the Avocado integration top ([`cohestra_packages/avocado/avocado_top.sv`](../cohestra_packages/avocado/avocado_top.sv)): a coherent CHI mesh + 4×L2 DUT, with boundary pins `clock`, high-active `reset`, 4× CCHI Type-1 port groups `cchi_t1p{0..3}_*`, 2× AXI memory-master outputs `axi_m{0,1}_*` (to the harness memory models) and 1× AXI slave input `axi_s0_*` (from the harness master/boot sequencer to the mesh config port). No CHI pins at the boundary.

## Architecture

```
  VCS process (simv)
 ┌──────────────────────────────────────────────────────────────┐
 │  cohestra_vcs_bridge.sv (no free-running clock; 1ps/1ps)     │
 │                                                              │
 │   initial: plusargs ──► import CohestraVCS_Main ──────────┐  │
 │                                                           │  │
 │   export task CohestraVCS_DriveLow(drv)   ◄── pins+clock=0,│  │
 │     #500 (settle)                                         │  │
 │   export func CohestraVCS_Sample(out)     ◄── zero-time   │  │
 │   export task CohestraVCS_DriveHigh()     ◄── clock=1,    │  │
 │     #500 (posedge)                                        │  │
 │   $dumpvars / $fsdbDumpvars (waveforms)                   │  │
 │                          ▲                                │  │
 │   `COHESTRA_VCS_TOP_MODULE u_dut (avocado_top: mesh+DUT)  │  │
 └──────────────────────────┼────────────────────────────────┼──┘
                            │ DPI (packed structs,           │
                            │  svBitVecVal* — bit-for-bit    │
                            │  contract, see cohestra_vcs_dpi│
                            │  .svh / .hpp)                  │
 ┌──────────────────────────┴────────────────────────────────┼──┐
 │  C++ harness (libcohestra_vcs.a + cohestra_vcs_dpi.cpp)   │  │
 │                                                           ▼  │
 │   CohestraVCS_Main → RunHarness (executable.cpp, cloned    │
 │   from cohestra_v3): CohestraVCSTop (vcs_top.hpp, POD pin  │
 │   shadow) + V3CCHIInterface / V3AXISlave / V3AXIMaster /   │
 │   V3BootHost instantiated unchanged. eval() = DriveLow +   │
 │   Sample (clock==0) / DriveHigh (clock==1).                │
 └────────────────────────────────────────────────────────────┘
```

Tick discipline is exactly the Verilator flow's: 2 ticks per clock cycle (drive → settle/sample → posedge). One harness time unit = one half cycle = 500 ps of simulator time (HP=500, `timescale 1ps/1ps`, a 1 ns clock).

## Build

Prerequisite: VCS installed and licensed (`vcs` on `PATH` or `VCS_HOME`; `SNPSLMD_LICENSE_FILE`/`LM_LICENSE_FILE`). **First time on a machine, run the smoke:**

```bash
smoke/run_vcs_smoke.sh        # staged go/no-go: PATH→VERSION→LICENSE→GCC→COMPILE→RUN→PASS (+optional FSDB)
```

Build (Avocado top):

```bash
# 1. (if not done for the Verilator flow) generate the mesh+DUT filelist:
(cd ../cohestra_packages/avocado/scripts && \
    AVOCADO_MESH_ROOT=/path/to/mesh/render ./gen_filelist.sh <dut rtl files>)

# 2. harness archive + simv (keep BUILD_DIR on a native filesystem — a
#    synced/network-mounted CHIron tree is very slow to build in):
BUILD_DIR=/path/to/native/build/cohestra_vcs \
    ./build.sh            # or: ./build.sh harness && ./build.sh simv

# 3. run:
(cd /path/to/native/build/cohestra_vcs && ./simv \
    +cohestra_config=<config.ini> +cohestra_cycles=200000)
```

`build.sh` env knobs: `CHIRON`, `AVOCADO_PKG`, `AVOCADO_MESH_ROOT` (filelist regeneration only), `BUILD_DIR`, `CYCLES`, `COHESTRA_VCS_TOP_MODULE`, `AVOCADO_DUT_MODULE`, `FILELIST`, `TOP_SOURCES`, `VCS_HOME`, `VERDI_HOME` (enables FSDB), `EXTRA_VCS_FLAGS`, `EXTRA_VCS_CFLAGS` — see the header of `build.sh`. These mirror the XSCache driver's (`scripts/avocado/boot_avocado.sh`) `CHIRON`/`AVOCADO_MESH_ROOT`/`BUILD_DIR`/`CYCLES` expectations.

## Runtime plusargs

| Plusarg | Default | Meaning |
|---|---|---|
| `+cohestra_config=<path>` | (none) | INI configuration file (same schema as cohestra_v3) |
| `+cohestra_cycles=<n>` | `200000` | harness-time budget (2 units per clock; 0 = unlimited) |
| `+cohestra_vcd=<path>` | (none) | dump VCD (when the INI does not enable dumping) |
| `+cohestra_fsdb=<path>` | (none) | dump FSDB (requires a `VERDI_HOME` build) |
| `+cohestra_log_file=<path>` | auto-named | harness log file |
| `+cohestra_no_log_file` | off | console-only logging |

Waveforms: the same `[verilated] waveform.*` INI keys are read, but `waveform.type` accepts `vcd`/`fsdb` only — **`fst` is rejected** (hard error when `waveform.enable=1`, warning otherwise; the stock cohestra_v3 configs that carry `waveform.type = fst` with `enable = 0` still run). Dumping is done by the simulator (`$dumpvars` / `$fsdbDumpvars` on the whole bridge, `waveform.start` honored via `$dumpoff`/`$dumpon` at `start × 500 ps`).

Exit status: the harness exit code (`0` ok, `1` startup/config, `2` verification) is printed as `COHESTRA_VCS_EXIT_STATUS=<n>`; simv exits 0 on success and non-zero on failure (via `$fatal`).

## Files

| File | Role |
|---|---|
| `executable.cpp` | harness logic (clone of `cohestra_v3/executable.cpp`; only Verilator touchpoints swapped) |
| `vcs_top.hpp` | `CohestraVCSTop`: POD shadow of the boundary pins; satisfies the cohestra_v3 pin traits (compile-time checked) |
| `cohestra_vcs_dpi.svh` | DPI `define`s, packed pin-snapshot structs, import/export declarations |
| `cohestra_vcs_dpi.hpp/.cpp` | C++ mirror structs (bit-for-bit contract), DPI exchange, `CohestraVCS_Main` |
| `waves_vcs.hpp/.cpp` | `[verilated] waveform.*` keys → simulator-side VCD/FSDB dumping |
| `cohestra_vcs_bridge.sv` | SV bridge: DUT instantiation, pin (un)packing, exported drive/sample tasks, initial entry |
| `CMakeLists.txt` | harness archive (`libcohestra_vcs.a`); no Verilator dependency |
| `build.sh` | VCS detection + harness build + simv link |
| `smoke/` | B0 environment smoke (`run_vcs_smoke.sh`) |

## Notes & status

- **VCS is not yet installed on this machine — the VCS build/run is untested.** Host-side verification done: C++ syntax/semantic checks (g++ `-fsyntax-only`, C++20), Verilator lint of the SV (bridge + smoke), CMake exclusion proofs. The DPI packed-struct layout is guarded at runtime (`CohestraVCS_CheckLayout` compares SV `$bits` against C++ `sizeof`).
- 4-state note: VCS X-initializes state where the Verilator model zero-initializes; sampled pins collapse X/Z to 0 at the DPI boundary. For Avocado configs use `reset_preamble = 0` with `reset_repeat = 2` (as the XSCache smoke configs do) so no X escapes the pre-reset window.
- `CohestraVCS_Main` is an imported DPI **task** (not a function): it calls the time-consuming exported tasks, which an imported-function call chain may not do (IEEE 1800 DPI).
- When editing the structs: the SV packed structs (`cohestra_vcs_dpi.svh`) and the C++ mirrors (`cohestra_vcs_dpi.hpp`) must match bit for bit — the SV side declares fields MSB-first (natural pin order), the C++ side in reverse order (word 0 first). Details in either file's header comment.
