//*****************************************************************************
// cohestra_vcs_dpi.svh — DPI layer of the Cohestra VCS harness: defines,
// packed pin-snapshot struct typedefs, and the import/export declarations.
//
// This header is `include'd INSIDE `module cohestra_vcs_bridge` (see
// cohestra_vcs_bridge.sv), so everything below is module-scope there: the
// import prototypes bind to the C++ implementations in cohestra_vcs_dpi.cpp,
// and the export declarations bind to the task/function bodies defined in the
// bridge module (each export declaration must sit in the same scope as the
// task/function it exports, hence they cannot live at $unit).
//
// ----------------------------------------------------------------------------
// TICK MAPPING (mirrors the Verilator cohestra_v3 flow, 2 ticks per cycle):
//
//   cohestra_v3 flow (executable.cpp)        VCS (this bridge)
//   --------------------------------------   --------------------------------
//   TickPreHandshake (drive pins)            C++ writes the shadow-top members
//   eval with clock=0 (comb settle)          export task  CohestraVCS_DriveLow
//                                            (blocking-assign all driven pins,
//                                             clock=0, #HP settle)
//   TickPostHandshake (sample + retire)      export func  CohestraVCS_Sample
//                                            (zero-time read of the settled
//                                             pins; called at the END of the
//                                             clock==0 eval, before the C++
//                                             TickPostHandshake runs)
//   eval with clock=1 (posedge)              export task  CohestraVCS_DriveHigh
//                                            (clock=1, #HP: the posedge)
//
// There is no free-running SV clock: C++ owns the clock through these calls.
// Time advances only inside the exported tasks (#HP per half cycle). Timescale
// is 1ps/1ps and HP=500, i.e. a 1ns clock; one harness time unit (one tick)
// therefore corresponds to 500ps of simulator time (2 ticks = 1000ps = 1
// cycle), matching the Verilator flow's time bookkeeping.
//
// ----------------------------------------------------------------------------
// WIRE-FORMAT CONTRACT — the packed structs below cross the DPI boundary as
// svBitVecVal word arrays (word 0 = least significant 32 bits). Both sides
// must match BIT FOR BIT:
//
//   * Every field is a whole number of 32-bit words — there is NO sub-word
//     packing anywhere. A 1-bit pin occupies a full 32-bit word.
//   * In a SystemVerilog packed struct the FIRST declared field occupies the
//     MOST significant bits, so the LAST declared field lands on word 0.
//   * The C++ mirror structs (cohestra_vcs_dpi.hpp) therefore declare the
//     same fields in REVERSE order, so that the first C++ member sits at byte
//     offset 0 == svBitVecVal word 0.
//   * A `bit [63:0]` field (48-bit addresses) maps to TWO consecutive C++
//     uint32_t members, <name>Lo then <name>Hi (little word order).
//   * A `bit [255:0]` field (256-bit data) maps to C++ uint32_t[8], element
//     [0] = bits [31:0] (Verilator VlWide word order).
//   * All fields are 2-state `bit` types: the DPI representation is
//     svBitVecVal (not svLogicVecVal) and any X/Z on a sampled pin collapses
//     to 0 — the same 2-state semantics the Verilator model has.
//   * All C++ members are uint32_t/uint32_t[N]: natural 4-byte alignment, no
//     struct padding on any mainstream compiler.
//
// The runtime guard: the bridge calls CohestraVCS_CheckLayout($bits(to_dut),
// $bits(from_dut)) before starting the harness; the C++ side compares against
// 8*sizeof(...) of its mirrors and refuses to run on mismatch.
//*****************************************************************************

`ifndef COHESTRA_VCS_DPI_SVH
`define COHESTRA_VCS_DPI_SVH

// Half period in timescale units (`timescale 1ps/1ps): 500 = 500ps, a 1ns
// (1GHz) clock. One harness tick = 500ps; 2 ticks per cycle, exactly matching
// the cohestra_v3 Verilator flow's 2-time-units-per-cycle convention.
`ifndef COHESTRA_VCS_HALF_PERIOD
`define COHESTRA_VCS_HALF_PERIOD 500
`endif

// Default DUT top module instantiated by the bridge.
`ifndef COHESTRA_VCS_TOP_MODULE
`define COHESTRA_VCS_TOP_MODULE avocado_top
`endif

// CHI monitor taps: define COHESTRA_VCS_CHI_MON (VCS +define+, and
// -DCOHESTRA_VCS_CHI_MON=1 for BOTH the VCS -CFLAGS and the CMake harness
// archive so every translation unit agrees) when the DUT top exports the
// passive mon_chi_rn0..3_* pins (avocado_top does; l2openllc_vcs_top does
// NOT — its build must leave this undefined). The define extends the
// from-DUT exchange struct below and makes the bridge connect and sample
// the taps; without it the bridge compiles exactly as before.

// Waveform type codes exchanged with CohestraVCS_OpenWaveform.
`define COHESTRA_VCS_WAVE_NONE 0
`define COHESTRA_VCS_WAVE_VCD  1
`define COHESTRA_VCS_WAVE_FSDB 2

//*****************************************************************************
// Packed pin-snapshot structs.
//
// Field declaration order below is the NATURAL pin order (the same order as
// avocado_dut_if.svh / top_dummy.sv) — which is MSB-first in the packed image.
// The C++ mirrors declare every struct in exact reverse order. The word index
// of each field (svBitVecVal word number == C++ byte offset / 4) is noted in
// the right-hand comments; check both files together when editing.
//*****************************************************************************

// -----------------------------------------------------------------------------
// CCHI Type-1 port, harness -> DUT (the DUT's rx* channels + tx* readys).
// 58 words = 1856 bits.
// -----------------------------------------------------------------------------
typedef struct packed {
    // rxevt                                                  word (C++ side)
    bit [31:0]  rxevt_valid;                              // 57
    bit [31:0]  rxevt_bits_TxnID;                         // 56
    bit [31:0]  rxevt_bits_SrcID;                         // 55
    bit [31:0]  rxevt_bits_TgtID;                         // 54
    bit [31:0]  rxevt_bits_Opcode;                        // 53
    bit [63:0]  rxevt_bits_Addr;                          // Hi=52, Lo=51
    bit [31:0]  rxevt_bits_NS;                            // 50
    bit [31:0]  rxevt_bits_MemAttr;                       // 49
    bit [31:0]  rxevt_bits_WayValid;                      // 48
    bit [31:0]  rxevt_bits_Way;                           // 47
    bit [31:0]  rxevt_bits_TraceTag;                      // 46
    // rxreq
    bit [31:0]  rxreq_valid;                              // 45
    bit [31:0]  rxreq_bits_TxnID;                         // 44
    bit [31:0]  rxreq_bits_SrcID;                         // 43
    bit [31:0]  rxreq_bits_TgtID;                         // 42
    bit [31:0]  rxreq_bits_Opcode;                        // 41
    bit [31:0]  rxreq_bits_Size;                          // 40
    bit [63:0]  rxreq_bits_Addr;                          // Hi=39, Lo=38
    bit [31:0]  rxreq_bits_TagAlias;                      // 37
    bit [31:0]  rxreq_bits_NS;                            // 36
    bit [31:0]  rxreq_bits_Order;                         // 35
    bit [31:0]  rxreq_bits_MemAttr;                       // 34
    bit [31:0]  rxreq_bits_Excl;                          // 33
    bit [31:0]  rxreq_bits_ExpCompData;                   // 32
    bit [31:0]  rxreq_bits_TraceTag;                      // 31
    bit [31:0]  rxreq_bits_WayValid;                      // 30
    bit [31:0]  rxreq_bits_Way;                           // 29
    // rxrsp
    bit [31:0]  rxrsp_valid;                              // 28
    bit [31:0]  rxrsp_bits_TxnID;                         // 27
    bit [31:0]  rxrsp_bits_SrcID;                         // 26
    bit [31:0]  rxrsp_bits_TgtID;                         // 25
    bit [31:0]  rxrsp_bits_Opcode;                        // 24
    bit [31:0]  rxrsp_bits_RespErr;                       // 23
    bit [31:0]  rxrsp_bits_Resp;                          // 22
    bit [31:0]  rxrsp_bits_TraceTag;                      // 21
    // rxdat
    bit [31:0]  rxdat_valid;                              // 20
    bit [31:0]  rxdat_bits_TxnID;                         // 19
    bit [31:0]  rxdat_bits_SrcID;                         // 18
    bit [31:0]  rxdat_bits_TgtID;                         // 17
    bit [31:0]  rxdat_bits_Opcode;                        // 16
    bit [31:0]  rxdat_bits_RespErr;                       // 15
    bit [31:0]  rxdat_bits_Resp;                          // 14
    bit [31:0]  rxdat_bits_DataID;                        // 13
    bit [255:0] rxdat_bits_Data;                          // 12..5 ([0]=LSW first)
    bit [31:0]  rxdat_bits_BE;                            // 4
    bit [31:0]  rxdat_bits_TraceTag;                      // 3
    // tx-channel readys (harness is the receiver)
    bit [31:0]  txsnp_ready;                              // 2
    bit [31:0]  txrsp_ready;                              // 1
    bit [31:0]  txdat_ready;                              // 0
} cohestra_vcs_cchi_t1_drive_t;

// -----------------------------------------------------------------------------
// CCHI Type-1 port, DUT -> harness (the DUT's tx* channels + rx* readys).
// 47 words = 1504 bits.
// -----------------------------------------------------------------------------
typedef struct packed {
    // rx-channel readys (DUT is the receiver)                word (C++ side)
    bit [31:0]  rxevt_ready;                              // 46
    bit [31:0]  rxreq_ready;                              // 45
    bit [31:0]  rxrsp_ready;                              // 44
    bit [31:0]  rxdat_ready;                              // 43
    // txsnp
    bit [31:0]  txsnp_valid;                              // 42
    bit [31:0]  txsnp_bits_TxnID;                         // 41
    bit [31:0]  txsnp_bits_SrcID;                         // 40
    bit [31:0]  txsnp_bits_TgtID;                         // 39
    bit [31:0]  txsnp_bits_Opcode;                        // 38
    bit [63:0]  txsnp_bits_Addr;                          // Hi=37, Lo=36
    bit [31:0]  txsnp_bits_NS;                            // 35
    bit [31:0]  txsnp_bits_TraceTag;                      // 34
    // txrsp
    bit [31:0]  txrsp_valid;                              // 33
    bit [31:0]  txrsp_bits_TxnID;                         // 32
    bit [31:0]  txrsp_bits_SrcID;                         // 31
    bit [31:0]  txrsp_bits_TgtID;                         // 30
    bit [31:0]  txrsp_bits_DBID;                          // 29
    bit [31:0]  txrsp_bits_Opcode;                        // 28
    bit [31:0]  txrsp_bits_RespErr;                       // 27
    bit [31:0]  txrsp_bits_Resp;                          // 26
    bit [31:0]  txrsp_bits_CBusy;                         // 25
    bit [31:0]  txrsp_bits_WayValid;                      // 24
    bit [31:0]  txrsp_bits_Way;                           // 23
    bit [31:0]  txrsp_bits_TraceTag;                      // 22
    // txdat
    bit [31:0]  txdat_valid;                              // 21
    bit [31:0]  txdat_bits_TxnID;                         // 20
    bit [31:0]  txdat_bits_SrcID;                         // 19
    bit [31:0]  txdat_bits_TgtID;                         // 18
    bit [31:0]  txdat_bits_DBID;                          // 17
    bit [31:0]  txdat_bits_Opcode;                        // 16
    bit [31:0]  txdat_bits_RespErr;                       // 15
    bit [31:0]  txdat_bits_Resp;                          // 14
    bit [31:0]  txdat_bits_DataSource;                    // 13
    bit [31:0]  txdat_bits_CBusy;                         // 12
    bit [31:0]  txdat_bits_DataID;                        // 11
    bit [255:0] txdat_bits_Data;                          // 10..3 ([0]=LSW first)
    bit [31:0]  txdat_bits_WayValid;                      // 2
    bit [31:0]  txdat_bits_Way;                           // 1
    bit [31:0]  txdat_bits_TraceTag;                      // 0
} cohestra_vcs_cchi_t1_sample_t;

// -----------------------------------------------------------------------------
// AXI4 "push" channels: AW + W + B-ready + AR + R-ready (27 words = 864 bits).
// Used for: axi_m<P> DUT -> harness (mesh memory master outputs) and
//           axi_s0   harness -> DUT (harness master model drives).
// -----------------------------------------------------------------------------
typedef struct packed {
    // AW                                                        word (C++ side)
    bit [31:0]  awvalid;                                    // 26
    bit [31:0]  awid;                                       // 25
    bit [63:0]  awaddr;                                     // Hi=24, Lo=23
    bit [31:0]  awlen;                                      // 22
    bit [31:0]  awsize;                                     // 21
    bit [31:0]  awburst;                                    // 20
    // W
    bit [31:0]  wvalid;                                     // 19
    bit [255:0] wdata;                                      // 18..11 ([0]=LSW first)
    bit [31:0]  wstrb;                                      // 10
    bit [31:0]  wlast;                                      // 9
    // B ready
    bit [31:0]  bready;                                     // 8
    // AR
    bit [31:0]  arvalid;                                    // 7
    bit [31:0]  arid;                                       // 6
    bit [63:0]  araddr;                                     // Hi=5, Lo=4
    bit [31:0]  arlen;                                      // 3
    bit [31:0]  arsize;                                     // 2
    bit [31:0]  arburst;                                    // 1
    // R ready
    bit [31:0]  rready;                                     // 0
} cohestra_vcs_axi_push_t;

// -----------------------------------------------------------------------------
// AXI4 "pull" channels: AW-ready + W-ready + B + AR-ready + R
// (18 words = 576 bits).
// Used for: axi_m<P> harness -> DUT (harness memory model drives) and
//           axi_s0   DUT -> harness (mesh config slave outputs).
// -----------------------------------------------------------------------------
typedef struct packed {
    // AW/W ready                                                word (C++ side)
    bit [31:0]  awready;                                    // 17
    bit [31:0]  wready;                                     // 16
    // B
    bit [31:0]  bvalid;                                     // 15
    bit [31:0]  bid;                                        // 14
    bit [31:0]  bresp;                                      // 13
    // AR ready
    bit [31:0]  arready;                                    // 12
    // R
    bit [31:0]  rvalid;                                     // 11
    bit [31:0]  rid;                                        // 10
    bit [255:0] rdata;                                      // 9..2 ([0]=LSW first)
    bit [31:0]  rresp;                                      // 1
    bit [31:0]  rlast;                                      // 0
} cohestra_vcs_axi_pull_t;

// -----------------------------------------------------------------------------
// Passive CHI-E RN monitor taps of one port (DUT -> harness only; never
// driven). Present iff COHESTRA_VCS_CHI_MON (avocado_top exports the pins;
// l2openllc_vcs_top does not).
// Per channel: the 1-bit flitpend/flitv/lcrdv taps plus the packed flit.
// The flit fields follow the PIN widths rounded up to whole 32-bit words
// (CHI-E: REQ 162b -> 6 words, RSP 73b -> 3, SNP 115b -> 4, DAT 422b -> 14)
// — NOT the harness's configured CHI flit widths.
// 62 words = 1984 bits per port.
// -----------------------------------------------------------------------------
`ifdef COHESTRA_VCS_CHI_MON
typedef struct packed {
    // txreq                                                  word (C++ side)
    bit [31:0]   txreq_flitpend;                            // 61
    bit [31:0]   txreq_flitv;                               // 60
    bit [31:0]   txreq_lcrdv;                               // 59
    bit [191:0]  txreq_flit;                                // 58..53 ([0]=LSW first)
    // txrsp
    bit [31:0]   txrsp_flitpend;                            // 52
    bit [31:0]   txrsp_flitv;                               // 51
    bit [31:0]   txrsp_lcrdv;                               // 50
    bit [95:0]   txrsp_flit;                                // 49..47
    // txdat
    bit [31:0]   txdat_flitpend;                            // 46
    bit [31:0]   txdat_flitv;                               // 45
    bit [31:0]   txdat_lcrdv;                               // 44
    bit [447:0]  txdat_flit;                                // 43..30
    // rxsnp
    bit [31:0]   rxsnp_flitpend;                            // 29
    bit [31:0]   rxsnp_flitv;                               // 28
    bit [31:0]   rxsnp_lcrdv;                               // 27
    bit [127:0]  rxsnp_flit;                                // 26..23
    // rxrsp
    bit [31:0]   rxrsp_flitpend;                            // 22
    bit [31:0]   rxrsp_flitv;                               // 21
    bit [31:0]   rxrsp_lcrdv;                               // 20
    bit [95:0]   rxrsp_flit;                                // 19..17
    // rxdat
    bit [31:0]   rxdat_flitpend;                            // 16
    bit [31:0]   rxdat_flitv;                               // 15
    bit [31:0]   rxdat_lcrdv;                               // 14
    bit [447:0]  rxdat_flit;                                // 13..0
} cohestra_vcs_chi_mon_sample_t;
`endif

// -----------------------------------------------------------------------------
// Top-level exchange structs (avocado_top boundary):
//   to_dut:   reset + 4x cchi_t1 drive + axi_m0/axi_m1 pull + axi_s0 push
//             = 1 + 4*58 + 2*18 + 27 = 296 words = 9472 bits
//   from_dut: 4x cchi_t1 sample + axi_m0/axi_m1 push + axi_s0 pull
//             = 4*47 + 2*27 + 18 = 260 words = 8320 bits
//             (+ 4x 62 = 248 words = 7936 bits of CHI monitor taps when
//              COHESTRA_VCS_CHI_MON is set: 508 words = 16256 bits total)
// clock is NOT part of the structs: the DriveLow/DriveHigh tasks own it.
// -----------------------------------------------------------------------------
typedef struct packed {
    bit [31:0]                  reset;          // word 295
    cohestra_vcs_cchi_t1_drive_t cchi_t1p0;     // words 294..237
    cohestra_vcs_cchi_t1_drive_t cchi_t1p1;     // words 236..179
    cohestra_vcs_cchi_t1_drive_t cchi_t1p2;     // words 178..121
    cohestra_vcs_cchi_t1_drive_t cchi_t1p3;     // words 120..63
    cohestra_vcs_axi_pull_t      axi_m0;        // words 62..45
    cohestra_vcs_axi_pull_t      axi_m1;        // words 44..27
    cohestra_vcs_axi_push_t      axi_s0;        // words 26..0
} cohestra_vcs_to_dut_t;

typedef struct packed {
`ifdef COHESTRA_VCS_CHI_MON
    // CHI monitor taps (avocado_top only), prepended on the MSB side so every
    // pre-existing field below keeps its word index.
    cohestra_vcs_chi_mon_sample_t chi_mon_p0;   // words 507..446
    cohestra_vcs_chi_mon_sample_t chi_mon_p1;   // words 445..384
    cohestra_vcs_chi_mon_sample_t chi_mon_p2;   // words 383..322
    cohestra_vcs_chi_mon_sample_t chi_mon_p3;   // words 321..260
`endif
    cohestra_vcs_cchi_t1_sample_t cchi_t1p0;    // words 259..213
    cohestra_vcs_cchi_t1_sample_t cchi_t1p1;    // words 212..166
    cohestra_vcs_cchi_t1_sample_t cchi_t1p2;    // words 165..119
    cohestra_vcs_cchi_t1_sample_t cchi_t1p3;    // words 118..72
    cohestra_vcs_axi_push_t       axi_m0;       // words 71..45
    cohestra_vcs_axi_push_t       axi_m1;       // words 44..18
    cohestra_vcs_axi_pull_t       axi_s0;       // words 17..0
} cohestra_vcs_from_dut_t;

//*****************************************************************************
// Imported from C++ (implemented in cohestra_vcs_dpi.cpp).
//*****************************************************************************

// Layout guard: called once before the harness starts. Returns 0 when the
// SV-side $bits of the two exchange structs match the C++ sizeof() mirrors.
import "DPI-C" context function int CohestraVCS_CheckLayout(
    input   int                 toDutBits,
    input   int                 fromDutBits
);

// The harness entry. Called ONCE from the bridge's initial block; the whole
// simulation runs inside this call (C++ owns the clock through the exported
// tasks below). Declared a *task*, not a function: an imported function's
// call chain is not allowed to block, and this entry calls the exported
// tasks DriveLow/DriveHigh which consume time (#HP). IEEE 1800 forbids
// calling time-consuming exported tasks from an imported-function context.
//   configPath    : +cohestra_config=<path>    ("" = no INI file)
//   maximumCycles : +cohestra_cycles=<n>       (harness-time budget, 2 units
//                                               per cycle; 0 = unlimited)
//   vcdPath       : +cohestra_vcd=<path>       ("" = none; used only when the
//   fsdbPath      :  +cohestra_fsdb=<path>      INI does not enable dumping)
//   logFilePath   : +cohestra_log_file=<path>  ("" = default log file name)
//   noLogFile     : +cohestra_no_log_file      (1 = console-only logging)
//   status        : harness exit code (0 ok, 1 startup/config, 2 verification)
import "DPI-C" context task CohestraVCS_Main(
    input   string              configPath,
    input   longint unsigned    maximumCycles,
    input   string              vcdPath,
    input   string              fsdbPath,
    input   string              logFilePath,
    input   int                 noLogFile,
    output  int                 status
);

//*****************************************************************************
// Exported to C++ (task/function bodies live in the bridge module; the C++
// prototypes are in cohestra_vcs_dpi.hpp — keep them in lockstep).
//*****************************************************************************

// Drive phase: blocking-assign every harness-driven pin from `drv`, pull
// clock low, then wait one half period so the whole DUT settles through the
// low phase (the Verilator flow's "eval with clock=0").
export "DPI-C" task CohestraVCS_DriveLow;

// Sample phase (zero time): read the settled pins into `out_s`. Called right
// after DriveLow returns, before the C++ TickPostHandshake runs.
export "DPI-C" function CohestraVCS_Sample;

// Posedge phase: pull clock high and wait one half period (the Verilator
// flow's "eval with clock=1"). Pins hold their driven values.
export "DPI-C" task CohestraVCS_DriveHigh;

// Waveform control, called from the C++ harness when [verilated] waveform.*
// (or the +cohestra_vcd/+cohestra_fsdb plusargs) enables dumping.
//   waveType  : `COHESTRA_VCS_WAVE_VCD / `COHESTRA_VCS_WAVE_FSDB
//   path      : output file path
//   startTick : first harness time unit to dump (0 = from the start);
//               implemented with $dumpoff/$dumpon at startTick*HP.
export "DPI-C" task CohestraVCS_OpenWaveform;
export "DPI-C" task CohestraVCS_CloseWaveform;

`endif // COHESTRA_VCS_DPI_SVH
