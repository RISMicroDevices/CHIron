# Avocado — mesh bring-up boot sequencer (Cohestra V3)

Avocado replays the 3x2 mesh configuration write stream through a harness
AXI master wired to the mesh RN-I config port: HN-F SAM control (SN node
IDs), two-SN mode, HN-F power-up (ON/FAM), RN-SAM system-cache region,
hashed HN target table, non-hashed MMIO window, and finally RN-SAM unstall.

## Layout

- `avocado_boot_script.h/.cpp/.inc` — the boot script: write+verify
  entries. The `.inc` is configuration data trace.
- `avocado_script_sequencer.hpp/.cpp` — the duck-typed replay engine
  (`AvocadoScriptSequencer<MasterT>`): per entry, `Write32` → wait B (must
  be OKAY) → `Read32` verify; one entry in flight.
- `../../../cohestra_bootings/boot_avocado.hpp` — the Boot-Stage adapter
  (`AvocadoBootSequencer`) registered in the Cohestra V3 harness.

## Usage

```ini
[boot]
sequencer = Avocado
avocado.axi_master = 0          # index into the harness's AXI master ports
avocado.warmup_cycles = 25000   # optional; default 25000
```

`avocado.warmup_cycles` idles the sequencer after the boot stage starts,
before the first config write: the mesh's power-up ramp (CCB FAM broadcast
over the PUB fabric) takes ~16.5k cycles after reset release, and the config
port does not answer until it completes. A pending write during the ramp is
held by the mesh (AWREADY stays low) and is harmless, so the knob is a
determinism margin — set 0 to start immediately. Note the mesh also requires
a reset-assertion edge after some unreset runtime to initialize its PUB
credit state under Verilator; the harness provides it via
`[root] reset_preamble` (default 1024 cycles) — do not set it to 0 for this
target (or use `reset_repeat >= 2` instead).

A failed write/verify aborts the run before the stimulation stage
(`BootSequencer::HasBootFailed`).
