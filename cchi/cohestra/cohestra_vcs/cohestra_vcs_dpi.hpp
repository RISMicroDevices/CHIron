//*****************************************************************************
// cohestra_vcs_dpi.hpp — C++ side of the Cohestra VCS DPI layer: the mirror
// structs of the SystemVerilog packed pin snapshots (cohestra_vcs_dpi.svh),
// the extern "C" prototypes of both directions, and the C++ exchange helpers.
//
// WIRE-FORMAT CONTRACT (must match cohestra_vcs_dpi.svh BIT FOR BIT):
//   * Every field is a whole number of 32-bit words; no sub-word packing.
//   * The SV packed structs declare fields MSB-first (natural pin order);
//     these C++ mirrors declare the same fields in REVERSE order, so the
//     first member below lands on svBitVecVal word 0 (byte offset 0).
//   * SV `bit [63:0]`  -> two C++ uint32_t: <name>Lo (lower word) then
//     <name>Hi. SV `bit [255:0]` -> uint32_t[8], element [0] = bits [31:0].
//   * All members are uint32_t / uint32_t[N]: 4-byte natural alignment, no
//     padding, sizeof() == word count * 4 on any mainstream compiler (this is
//     enforced by the static_asserts below and re-checked at runtime against
//     the SV-side $bits via CohestraVCS_CheckLayout).
//
// CHI MONITOR TAPS (COHESTRA_VCS_CHI_MON): when the DUT top exports the
// passive mon_chi_rn0..3_* pins (avocado_top does; l2openllc_vcs_top does
// not), FromDUT gains four ChiMonSample blocks. The define must reach EVERY
// translation unit that sees these headers (the VCS -CFLAGS compiling
// cohestra_vcs_dpi.cpp AND the CMake harness archive) as
// -DCOHESTRA_VCS_CHI_MON=1, plus VCS +define+COHESTRA_VCS_CHI_MON — the
// structs must stay bit-identical on both sides of DPI, and CohestraVCSTop
// (vcs_top.hpp) must stay layout-identical across all TUs (ODR).
//
// The C++ prototypes of the SV-exported tasks/functions are declared here
// rather than #include'ing the simulator-generated vc_hdrs.h, so this header
// stays tool-independent; the signatures must match the export declarations
// in cohestra_vcs_dpi.svh (DPI ABI: packed struct args are svBitVecVal
// pointers, `string` is const char*, `longint unsigned` is uint64_t).
//*****************************************************************************
#pragma once

#include <cstdint>

#include <svdpi.h>


namespace Cohestra::VCS::wire {

    // -------------------------------------------------------------------------
    // CCHI Type-1 port, harness -> DUT. Mirror of cohestra_vcs_cchi_t1_drive_t
    // (58 words). Word indices noted per field.
    // -------------------------------------------------------------------------
    struct CchiT1Drive {
        uint32_t txdat_ready;               // 0
        uint32_t txrsp_ready;               // 1
        uint32_t txsnp_ready;               // 2

        uint32_t rxdat_bits_TraceTag;       // 3
        uint32_t rxdat_bits_BE;             // 4
        uint32_t rxdat_bits_Data[8];        // 5..12 ([0] = LSW)
        uint32_t rxdat_bits_DataID;         // 13
        uint32_t rxdat_bits_Resp;           // 14
        uint32_t rxdat_bits_RespErr;        // 15
        uint32_t rxdat_bits_Opcode;         // 16
        uint32_t rxdat_bits_TgtID;          // 17
        uint32_t rxdat_bits_SrcID;          // 18
        uint32_t rxdat_bits_TxnID;          // 19
        uint32_t rxdat_valid;               // 20

        uint32_t rxrsp_bits_TraceTag;       // 21
        uint32_t rxrsp_bits_Resp;           // 22
        uint32_t rxrsp_bits_RespErr;        // 23
        uint32_t rxrsp_bits_Opcode;         // 24
        uint32_t rxrsp_bits_TgtID;          // 25
        uint32_t rxrsp_bits_SrcID;          // 26
        uint32_t rxrsp_bits_TxnID;          // 27
        uint32_t rxrsp_valid;               // 28

        uint32_t rxreq_bits_Way;            // 29
        uint32_t rxreq_bits_WayValid;       // 30
        uint32_t rxreq_bits_TraceTag;       // 31
        uint32_t rxreq_bits_ExpCompData;    // 32
        uint32_t rxreq_bits_Excl;           // 33
        uint32_t rxreq_bits_MemAttr;        // 34
        uint32_t rxreq_bits_Order;          // 35
        uint32_t rxreq_bits_NS;             // 36
        uint32_t rxreq_bits_TagAlias;       // 37
        uint32_t rxreq_bits_AddrLo;         // 38
        uint32_t rxreq_bits_AddrHi;         // 39
        uint32_t rxreq_bits_Size;           // 40
        uint32_t rxreq_bits_Opcode;         // 41
        uint32_t rxreq_bits_TgtID;          // 42
        uint32_t rxreq_bits_SrcID;          // 43
        uint32_t rxreq_bits_TxnID;          // 44
        uint32_t rxreq_valid;               // 45

        uint32_t rxevt_bits_TraceTag;       // 46
        uint32_t rxevt_bits_Way;            // 47
        uint32_t rxevt_bits_WayValid;       // 48
        uint32_t rxevt_bits_MemAttr;        // 49
        uint32_t rxevt_bits_NS;             // 50
        uint32_t rxevt_bits_AddrLo;         // 51
        uint32_t rxevt_bits_AddrHi;         // 52
        uint32_t rxevt_bits_Opcode;         // 53
        uint32_t rxevt_bits_TgtID;          // 54
        uint32_t rxevt_bits_SrcID;          // 55
        uint32_t rxevt_bits_TxnID;          // 56
        uint32_t rxevt_valid;               // 57
    };

    // -------------------------------------------------------------------------
    // CCHI Type-1 port, DUT -> harness. Mirror of cohestra_vcs_cchi_t1_sample_t
    // (47 words).
    // -------------------------------------------------------------------------
    struct CchiT1Sample {
        uint32_t txdat_bits_TraceTag;       // 0
        uint32_t txdat_bits_Way;            // 1
        uint32_t txdat_bits_WayValid;       // 2
        uint32_t txdat_bits_Data[8];        // 3..10 ([0] = LSW)
        uint32_t txdat_bits_DataID;         // 11
        uint32_t txdat_bits_CBusy;          // 12
        uint32_t txdat_bits_DataSource;     // 13
        uint32_t txdat_bits_Resp;           // 14
        uint32_t txdat_bits_RespErr;        // 15
        uint32_t txdat_bits_Opcode;         // 16
        uint32_t txdat_bits_DBID;           // 17
        uint32_t txdat_bits_TgtID;          // 18
        uint32_t txdat_bits_SrcID;          // 19
        uint32_t txdat_bits_TxnID;          // 20
        uint32_t txdat_valid;               // 21

        uint32_t txrsp_bits_TraceTag;       // 22
        uint32_t txrsp_bits_Way;            // 23
        uint32_t txrsp_bits_WayValid;       // 24
        uint32_t txrsp_bits_CBusy;          // 25
        uint32_t txrsp_bits_Resp;           // 26
        uint32_t txrsp_bits_RespErr;        // 27
        uint32_t txrsp_bits_Opcode;         // 28
        uint32_t txrsp_bits_DBID;           // 29
        uint32_t txrsp_bits_TgtID;          // 30
        uint32_t txrsp_bits_SrcID;          // 31
        uint32_t txrsp_bits_TxnID;          // 32
        uint32_t txrsp_valid;               // 33

        uint32_t txsnp_bits_TraceTag;       // 34
        uint32_t txsnp_bits_NS;             // 35
        uint32_t txsnp_bits_AddrLo;         // 36
        uint32_t txsnp_bits_AddrHi;         // 37
        uint32_t txsnp_bits_Opcode;         // 38
        uint32_t txsnp_bits_TgtID;          // 39
        uint32_t txsnp_bits_SrcID;          // 40
        uint32_t txsnp_bits_TxnID;          // 41
        uint32_t txsnp_valid;               // 42

        uint32_t rxdat_ready;               // 43
        uint32_t rxrsp_ready;               // 44
        uint32_t rxreq_ready;               // 45
        uint32_t rxevt_ready;               // 46
    };

    // -------------------------------------------------------------------------
    // AXI4 "push" channels (AW + W + bready + AR + rready), 27 words.
    // Mirror of cohestra_vcs_axi_push_t. Used for the axi_m<P> DUT->harness
    // sample and the axi_s0 harness->DUT drive.
    // -------------------------------------------------------------------------
    struct AxiPush {
        uint32_t rready;                    // 0
        uint32_t arburst;                   // 1
        uint32_t arsize;                    // 2
        uint32_t arlen;                     // 3
        uint32_t araddrLo;                  // 4
        uint32_t araddrHi;                  // 5
        uint32_t arid;                      // 6
        uint32_t arvalid;                   // 7
        uint32_t bready;                    // 8
        uint32_t wlast;                     // 9
        uint32_t wstrb;                     // 10
        uint32_t wdata[8];                  // 11..18 ([0] = LSW)
        uint32_t wvalid;                    // 19
        uint32_t awburst;                   // 20
        uint32_t awsize;                    // 21
        uint32_t awlen;                     // 22
        uint32_t awaddrLo;                  // 23
        uint32_t awaddrHi;                  // 24
        uint32_t awid;                      // 25
        uint32_t awvalid;                   // 26
    };

    // -------------------------------------------------------------------------
    // AXI4 "pull" channels (awready + wready + B + arready + R), 18 words.
    // Mirror of cohestra_vcs_axi_pull_t. Used for the axi_m<P> harness->DUT
    // drive and the axi_s0 DUT->harness sample.
    // -------------------------------------------------------------------------
    struct AxiPull {
        uint32_t rlast;                     // 0
        uint32_t rresp;                     // 1
        uint32_t rdata[8];                  // 2..9 ([0] = LSW)
        uint32_t rid;                       // 10
        uint32_t rvalid;                    // 11
        uint32_t arready;                   // 12
        uint32_t bresp;                     // 13
        uint32_t bid;                       // 14
        uint32_t bvalid;                    // 15
        uint32_t wready;                    // 16
        uint32_t awready;                   // 17
    };

    // -------------------------------------------------------------------------
    // Top-level exchange structs, mirrors of cohestra_vcs_to_dut_t /
    // cohestra_vcs_from_dut_t (avocado_top boundary).
    // -------------------------------------------------------------------------
#ifdef COHESTRA_VCS_CHI_MON
    // Passive CHI-E RN monitor taps of one port, DUT -> harness. Mirror of
    // cohestra_vcs_chi_mon_sample_t (62 words). The flit arrays follow the PIN
    // widths rounded up to whole words (CHI-E: REQ 162b -> 6 words, RSP 73b ->
    // 3, SNP 115b -> 4, DAT 422b -> 14), NOT the harness's configured CHI flit
    // widths — the pins are the mesh-side CHI-E flits.
    struct ChiMonSample {
        uint32_t rxdat_flit[14];            // 0..13 ([0] = LSW)
        uint32_t rxdat_lcrdv;               // 14
        uint32_t rxdat_flitv;               // 15
        uint32_t rxdat_flitpend;            // 16

        uint32_t rxrsp_flit[3];             // 17..19
        uint32_t rxrsp_lcrdv;               // 20
        uint32_t rxrsp_flitv;               // 21
        uint32_t rxrsp_flitpend;            // 22

        uint32_t rxsnp_flit[4];             // 23..26
        uint32_t rxsnp_lcrdv;               // 27
        uint32_t rxsnp_flitv;               // 28
        uint32_t rxsnp_flitpend;            // 29

        uint32_t txdat_flit[14];            // 30..43
        uint32_t txdat_lcrdv;               // 44
        uint32_t txdat_flitv;               // 45
        uint32_t txdat_flitpend;            // 46

        uint32_t txrsp_flit[3];             // 47..49
        uint32_t txrsp_lcrdv;               // 50
        uint32_t txrsp_flitv;               // 51
        uint32_t txrsp_flitpend;            // 52

        uint32_t txreq_flit[6];             // 53..58 ([0] = LSW)
        uint32_t txreq_lcrdv;               // 59
        uint32_t txreq_flitv;               // 60
        uint32_t txreq_flitpend;            // 61
    };
#endif

    struct ToDUT {
        AxiPush     axi_s0;                 // words 0..26  (harness master drives AW/W/bready/AR/rready)
        AxiPull     axi_m1;                 // words 27..44 (harness memory drives awready/wready/B/arready/R)
        AxiPull     axi_m0;                 // words 45..62
        CchiT1Drive cchi_t1p3;              // words 63..120
        CchiT1Drive cchi_t1p2;              // words 121..178
        CchiT1Drive cchi_t1p1;              // words 179..236
        CchiT1Drive cchi_t1p0;              // words 237..294
        uint32_t    reset;                  // word 295
    };

    struct FromDUT {
        AxiPull      axi_s0;                // words 0..17
        AxiPush      axi_m1;                // words 18..44
        AxiPush      axi_m0;                // words 45..71
        CchiT1Sample cchi_t1p3;             // words 72..118
        CchiT1Sample cchi_t1p2;             // words 119..165
        CchiT1Sample cchi_t1p1;             // words 166..212
        CchiT1Sample cchi_t1p0;             // words 213..259
#ifdef COHESTRA_VCS_CHI_MON
        // CHI monitor taps (the SV side prepends them on the MSB side, so
        // they sit above the pre-existing words, which keep their indices).
        ChiMonSample chi_mon_p3;            // words 260..321
        ChiMonSample chi_mon_p2;            // words 322..383
        ChiMonSample chi_mon_p1;            // words 384..445
        ChiMonSample chi_mon_p0;            // words 446..507
#endif
    };

    static_assert(sizeof(CchiT1Drive)  == 58  * 4, "CchiT1Drive must be 58 words");
    static_assert(sizeof(CchiT1Sample) == 47  * 4, "CchiT1Sample must be 47 words");
    static_assert(sizeof(AxiPush)      == 27  * 4, "AxiPush must be 27 words");
    static_assert(sizeof(AxiPull)      == 18  * 4, "AxiPull must be 18 words");
    static_assert(sizeof(ToDUT)        == 296 * 4, "ToDUT must be 296 words");
#ifdef COHESTRA_VCS_CHI_MON
    static_assert(sizeof(ChiMonSample) == 62  * 4, "ChiMonSample must be 62 words");
    static_assert(sizeof(FromDUT)      == 508 * 4, "FromDUT must be 508 words (260 + 248 monitor)");
#else
    static_assert(sizeof(FromDUT)      == 260 * 4, "FromDUT must be 260 words");
#endif

    // Waveform type codes (`COHESTRA_VCS_WAVE_*` in the .svh).
    inline constexpr uint32_t WAVE_NONE = 0;
    inline constexpr uint32_t WAVE_VCD  = 1;
    inline constexpr uint32_t WAVE_FSDB = 2;
}


//*****************************************************************************
// SV -> C++ (implemented in cohestra_vcs_dpi.cpp / executable.cpp).
//*****************************************************************************
extern "C" int  CohestraVCS_CheckLayout(int toDutBits, int fromDutBits);

extern "C" void CohestraVCS_Main(
    const char*         configPath,
    uint64_t            maximumCycles,
    const char*         vcdPath,
    const char*         fsdbPath,
    const char*         logFilePath,
    int                 noLogFile,
    int*                status);

//*****************************************************************************
// C++ -> SV (exported by the bridge module; the C++ side must set the scope
// to the bridge instance before calling — see cohestra_vcs_dpi.cpp).
//*****************************************************************************
extern "C" void CohestraVCS_DriveLow(const svBitVecVal* drv);
extern "C" void CohestraVCS_Sample(svBitVecVal* out_s);
extern "C" void CohestraVCS_DriveHigh();
extern "C" void CohestraVCS_OpenWaveform(uint32_t waveType, const char* path, uint64_t startTick);
extern "C" void CohestraVCS_CloseWaveform();


struct CohestraVCSTop;

namespace Cohestra::VCS {

    // Program arguments handed from the SV plusargs to the harness entry.
    struct HarnessArguments {
        const char* configPath      = nullptr;  // "" = none
        uint64_t    maximumCycles   = 200000;   // harness-time budget (0 = unlimited)
        const char* vcdPath         = nullptr;  // +cohestra_vcd ("" = none)
        const char* fsdbPath        = nullptr;  // +cohestra_fsdb ("" = none)
        const char* logFilePath     = nullptr;  // "" = default name
        bool        noLogFile       = false;
    };

    // The harness itself (cohestra_vcs/executable.cpp). Returns the harness
    // exit code (0 ok, 1 startup/config, 2 verification failure). May throw;
    // CohestraVCS_Main catches and maps to exit code 1.
    int     RunHarness(const HarnessArguments& args);

    // The DPI exchange behind CohestraVCSTop::eval() (cohestra_vcs_dpi.cpp).
    // clock==0: pack the drive struct, DriveLow, Sample, unpack into the
    //           shadow top (this is the "eval with clock=0" phase).
    // clock==1: DriveHigh (the posedge phase).
    void    EvalTop(CohestraVCSTop& top) noexcept;

    // Waveform control called by the VCS wave tracer glue (waves_vcs.cpp).
    bool    OpenWaveform(uint32_t waveType, const char* path, uint64_t startTick) noexcept;
    void    CloseWaveform() noexcept;

    // True when the DPI scope has been captured (i.e. CohestraVCS_Main is on
    // the call stack) and the exported tasks may be called.
    bool    IsBridgeBound() noexcept;
}

// Whether this build of the harness/bridge was linked with Verdi FSDB support
// (build.sh sets it on both the CMake archive and the VCS -CFLAGS side when
// VERDI_HOME is present).
#ifndef COHESTRA_VCS_HAVE_FSDB
#define COHESTRA_VCS_HAVE_FSDB 0
#endif
