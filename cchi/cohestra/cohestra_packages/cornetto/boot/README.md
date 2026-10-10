# Cornetto — mesh bring-up boot sequencer (Cohestra V3)

Cornetto replays the 1x3 mesh configuration write stream through a harness
AXI master wired to the mesh RN-I config port: HN-F SAM control (SN node
ID), HN-F power-up (ON/FAM), RN-SAM system-cache region, hashed HN target
table, non-hashed MMIO window, and finally RN-SAM unstall (external SAMs
only).

## Layout

- `cornetto_boot_script.h/.cpp/.inc` — the boot script: write+verify
  entries. The `.inc` is configuration data trace.
- `cornetto_script_sequencer.hpp/.cpp` — the duck-typed replay engine
  (`CornettoScriptSequencer<MasterT>`): per entry, `Write32` → wait B (must
  be OKAY) → `Read32` verify; one entry in flight.
- `../../../cohestra_bootings/boot_cornetto.hpp` — the Boot-Stage adapter
  (`CornettoBootSequencer`) registered in the Cohestra V3 harness.

## Usage

```ini
[boot]
sequencer = Cornetto
cornetto.axi_master = 0          # index into the harness's AXI master ports
cornetto.warmup_cycles = 25000   # optional; default 25000
```

`cornetto.warmup_cycles` idles the sequencer after the boot stage starts,
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

## Boot script decode (49 entries, all write+verify)

Address formation: `0x6000_0000` (CFGM periphbase — the `Cornetto` wrapper
straps `CFGM_PERIPHBASE=20'h06`) + config-block offset (from the mesh
render's testbench address-block YAML)
+ register offset (register maps: mesh TRM r3p0 §4.3.10 HN-F,
§4.3.16 RN-SAM registers; offsets cross-checked against this render's
generated register-define headers).

Blocks programmed: HNS (HN-F nid8) at block offset `0x4c0000`, the RN-I
(nid4) RN-SAM at `0x90000`, and the two external RNFEESAM RN-SAMs at
`0x8a0000` (nid16) / `0x9a0000` (nid20).

### HN-F group (0x604c0000 + offset) — 5 entries

| Address | Register | Value | Decode |
|---|---|---|---|
| 0x604c0d00/04 | `hns_sam_control` | 0x00000000_0000000c | `hn_cfg_sn0_nodeid`[10:0]=12 (the single SN: SBSX nid12), sn1/sn2=0, 3/5/6-SN enables=0. (Avocado: 0x00048004 = sn0=4, sn1=72 — its two SBSX; re-derived for Cornetto's single SBSX nid12.) |
| 0x604c0d28 | `hns_sam_control2` | 0x00000000 | `hn_cfg_two_sn_en`[0]=0 — single SN, no 2-SN striping (Avocado wrote 1). |
| 0x604c1c00/04 | `hns_ppu_pwpr` | 0x00000000_00000138 | `dyn_en`[8]=1, `op_mode`[7:4]=0b0011 (FAM), `policy`[3:0]=0b1000 (ON) — HN-F power-up to full associative mode. Same as Avocado. |

### RN-SAM group (per SAM base) — 8 registers

Programmed SAMs: `R` = RNI nid4 SAM (0x60090000), `E16` = external SAM
nid16 (0x608a0000), `E20` = external SAM nid20 (0x609a0000).

| Offset | Register | Value (64-bit) | SAMs | Decode |
|---|---|---|---|---|
| 0xE00/04 | `sys_cache_grp_region0` | 0x11000000_00000001 | R,E16,E20 | `valid`[0]=1, `target_type`[4:2]=0 (HN-F), `secure`[7:6]=0, `base_addr`[51:16]=0, `size`[62:56]=17 → SCG0 = [0x0, 2^(26+17)) = [0, 0x2_0000_0000) hashed onto the SCG0 HN table (size = log2(region) − `HTG_RCOMP_LSB`(26); RTL: `por_sam_pipe.v` size-based HTG compare). Same as Avocado. |
| 0x1000/04 | `sys_cache_grp_sn_nodeid_reg0` | 0x00000000_0000000c | E16,E20 | `sn_nodeid_0`[10:0]=12 — SN target table entry 0 = SBSX nid12 (Avocado: 0x00048004 = {4,72}). Skipped on the RNI SAM (mirroring Avocado's nid76 SAM: prefetch/SN-table use is disabled there — `por_rnsam_prefetch_en=0`). |
| 0xF00/04 | `sys_cache_grp_hn_nodeid_reg0` | 0x00000000_00000008 | R,E16,E20 | `nodeid_0`[10:0]=8 — hashed HN-F target table = {nid8} only (Avocado: 0x00000440_08024028 = {40,36,8,68}). |
| 0xEA0/04 | `sys_cache_group_hn_count` | 0x00000000_00000001 | R,E16,E20 | `scg0_num_hnf`[7:0]=1 — one HN-F in SCG0; the power-of-2 HN hash degenerates to index 0 (Avocado: 4). |
| 0xC00/04 | `non_hash_mem_region_reg0` | 0x00000000_10000005 | R,E16,E20 | `valid`[0]=1, `target_type`[4:2]=1 (HN-I), `secure`[7:6]=0, `base_addr`[51:16]=0x1000 → MMIO window base 0x1000_0000. Same as Avocado. |
| 0xCC0/04 | `non_hash_mem_region_cfg2_reg0` | 0x00000000_1fff0000 | R,E16,E20 | `end_addr`[51:16]=0x1fff → window [0x1000_0000, 0x1FFF_FFFF] (range compare, `NONHASH_RCOMP_EN=1` in this render). Targets the HN-D (nid0) downstream peripheral master — parked in this integration, so the window must not be used by traffic. Same as Avocado. |
| 0xD80/04 | `non_hash_tgt_nodeid0` | 0x00000000_00000000 | R,E16,E20 | `nodeid_0`[10:0]=0 — non-hash region 0 routes to nid0 (HND). Same as Avocado. |
| 0x1100/04 | `rnsam_status` | 0x10000000_00000002 | E16,E20 | `default_target_type`[62:60]=1 (HN-I), `default_nodeid`[58:48]=0, `dn_nodeid`[18:8]=0, `nstall_req`[1]=1 (UNSTALL requests), `use_default_node`[0]=0 (hash/select normally). Skipped on the RNI SAM (mirroring Avocado: the config-ingress SAM stays stalled; RN-I S2 config traffic bypasses the stall via the wrapper's `RNID_SAM_STALL_DIS_S2=1` strap). Same value as Avocado. |

Totals: HN-F 5 + RNI SAM 12 (6 regs × 2) + 2 × external SAM 16 (8 regs × 2)
= 49 entries. (Avocado: 4×5 + 4×16 + 1×12 = 96.)

Ordering follows Avocado: all HN-F writes first, then the RN-SAM registers
grouped by register across the SAMs, unstall last. Every access is a
single-beat 32-bit AXI write (64-bit registers as lo/hi pairs) followed by
a read-back compare.
