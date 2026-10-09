# Biscotto — mesh bring-up boot sequencer (Cohestra V3)

Biscotto replays the 2x2 mesh configuration write stream through a harness
AXI master wired to the mesh RN-I config port: HN-F SAM control (SN node
ID), single-SN mode, HN-F power-up (ON/FAM), RN-SAM system-cache region,
hashed HN target table, non-hashed MMIO window, and finally RN-SAM unstall.

The target is the 2x2 mesh render, referenced only via `BISCOTTO_MESH_ROOT`
(no default — point it at the local render checkout): 2x HN-F (node IDs 8 and
40, 512 KiB SLC each), 1x SBSX system node (node ID 36), 3x external RN-F
with XP-hosted RN-SAMs (node IDs 4, 12, 44), 1x RN-I with its own RN-SAM
(node ID 32 — the boot/config port), 1x HND (node ID 0). Config space is
PERIPHBASE `0x6000_0000`.

## Derivation provenance

**70 entries**, all write+verify, derived from the proven 96-entry Avocado
script (`cohestra_packages/avocado/boot/avocado_boot_script.inc`) with:

- the Biscotto address map (the render's testbench address-block YAML
  under `BISCOTTO_MESH_ROOT`), and
- the mesh TRM (r3p0) register offsets/fields.

64-bit registers are written as low/high 32-bit pairs, low word first, as
in Avocado. **Not yet proven on simulation at authoring time.**

Config-block bases used below (PERIPHBASE + offset from the address map):

| Block                     | Base         | Node            |
|---------------------------|--------------|-----------------|
| HNS (HN-F)                | `0x604c0000` | NID 8           |
| HNS (HN-F)                | `0x614c0000` | NID 40          |
| RNSAM (RNF-hosted)        | `0x601a0000` | NID 4           |
| RNSAM (RNF-hosted)        | `0x605a0000` | NID 12          |
| RNSAM (RNF-hosted)        | `0x615a0000` | NID 44          |
| RNSAM (RNI)               | `0x61080000` | NID 32          |

## Entry groups

| # | Register (offset) | Value (64-bit) | Targets | Meaning |
|---|-------------------|----------------|---------|---------|
| g1 | `hns_sam_control` (`0xD00`) | `0x00000000_00000024` | 2x HN-F | SN0 node ID = 36 (the single SN, SBSX NID36) in `hn_cfg_sn0_nodeid[10:0]`; no 3/5/6-SN enables, no top-address-bit hashing. |
| g2 | `hns_sam_control2` (`0xD28`) | `0x00000000` | 2x HN-F | Single-SN configuration: `hn_cfg_two_sn_en`/`four`/`eight` all clear (Avocado wrote 1 = two-SN). Written as one 32-bit entry per HN-F, as Avocado. |
| g3 | `hns_ppu_pwpr` (`0x1C00`) | `0x00000000_00000138` | 2x HN-F | Power-up: policy ON (`0x8`), op_mode FAM (`0x3`), `dyn_en` = 1. |
| g4 | `sys_cache_grp_region0` (`0xE00`) | `0x11000000_00000001` | 4x RNSAM | SCG0 valid, target type HN-F, base 0, size `0x11` (8 GiB encoding, log2(bytes)-16). |
| g5 | `sys_cache_grp_sn_nodeid_reg0` (`0x1000`) | `0x00000000_00000024` | 3x RNF RNSAM | SCG0 SN0 node ID = 36. **RNI RNSAM skipped — see below.** |
| g6 | `sys_cache_grp_hn_nodeid_reg0` (`0xF00`) | `0x00000000_00028008` | 4x RNSAM | HN-F target table: nodeid0 = 8 at `[10:0]`, nodeid1 = 40 at `[22:12]`. |
| g7 | `sys_cache_group_hn_count` (`0xEA0`) | `0x00000000_00000002` | 4x RNSAM | 2 HN-Fs in SCG0 (Avocado: 4). |
| g8 | `non_hash_mem_region_reg0` (`0xC00`) | `0x00000000_10000005` | 4x RNSAM | Non-hashed region0 valid, target type HN-I, base `0x1000_0000`. |
| g9 | `non_hash_mem_region_cfg2_reg0` (`0xCC0`) | `0x00000000_1fff0000` | 4x RNSAM | Region0 end `0x1fff_ffff` → non-hashed MMIO window `0x1000_0000`..`0x1fff_ffff` (256 MiB). |
| g10 | `non_hash_tgt_nodeid0` (`0xD80`) | `0x00000000_00000000` | 4x RNSAM | Non-hashed region0 target = node 0 (HND/HN-I, the downstream peripheral port). |
| g11 | `rnsam_status` (`0x1100`) | `0x10000000_00000002` | 3x RNF RNSAM | `nstall_req` = 1 (UNSTALL), `use_default_node` = 0 (hash per the programmed tables), `default_target_type` = HN-I, `default_nodeid` = 0 (HND). **RNI RNSAM skipped — see below.** |

Entry arithmetic: g1 2x2=4, g2 2x1=2, g3 2x2=4, g4 4x2=8, g5 3x2=6,
g6..g10 4x2=8 each, g11 3x2=6 → **70 entries** (Avocado: 96).

### Why the RNI RN-SAM (NID32) is skipped in g5 and g11

Mirroring Avocado, which skips its RNI RN-SAM (NID76) in the same two
groups:

- **g5** — the RNI-hosted RN-SAM is rendered *without SN-F targets*
  (`POR_RNSAM_NUM_SNF_TGT_PARAM=0` for the RNI instance in both renders,
  per the generated RAL model): `sys_cache_grp_sn_nodeid_reg0` does not
  exist in its register block. Only the RNF-hosted RN-SAMs (NID 4/12/44)
  carry the SN table. A write to the absent register would not verify.
- **g11** — the RNI RN-SAM is deliberately left at its reset value
  (`0x10000000_00000001`: stalled, `use_default_node` = 1, default target
  HN-I node 0). The RN-I port is the boot/config path; its traffic must
  keep routing deterministically to the HND (config/peripheral space)
  instead of being hashed into the coherent HN-F fabric. Only the RNF
  RN-SAMs — the CPU-cluster DRAM masters — are unstalled onto the hashed
  SCG0 tables.

## Layout

- `biscotto_boot_script.h/.cpp/.inc` — the boot script: write+verify
  entries. The `.inc` is configuration data trace.
- `biscotto_script_sequencer.hpp/.cpp` — the duck-typed replay engine
  (`BiscottoScriptSequencer<MasterT>`): per entry, `Write32` → wait B (must
  be OKAY) → `Read32` verify; one entry in flight.
- `../../../cohestra_bootings/boot_biscotto.hpp` — the Boot-Stage adapter
  (`BiscottoBootSequencer`) registered in the Cohestra V3 harness.

## Usage

```ini
[boot]
sequencer = Biscotto
biscotto.axi_master = 0          # index into the harness's AXI master ports
biscotto.warmup_cycles = 25000   # optional; default 25000
```

`biscotto.warmup_cycles` idles the sequencer after the boot stage starts,
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
