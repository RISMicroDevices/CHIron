//*****************************************************************************
// avocado_top.sv — Avocado integration top for Cohestra V3.
//
// Composes the Avocado mesh wrapper (module Avocado, provided by the mesh
// render; see scripts/gen_filelist.sh) with a user-supplied CCHI DUT
// (module `AVOCADO_DUT_MODULE, default avocado_dut — override with
// +define+AVOCADO_DUT_MODULE=<name>; see integration/avocado_dut_template.sv
// for the port contract), and exports exactly the Cohestra V3-standard pin
// groups:
//
//   clock / reset        global clock; reset is high-active (the wrapper's
//                        nRESET is driven with ~reset)
//   cchi_t1p0..3_*       4x Type-1 CCHI ports (pass-through to the DUT; the
//                        harness binds its Taurus upstream nodes here)
//   axi_m0_* / axi_m1_*  2x AXI4 memory ports, 256-bit data, 24-bit ID
//                        (harness is the slave/memory; driven by the mesh
//                        memory (SBSX) masters MEM4 / MEM72)
//   axi_s0_*             1x AXI4 config port, 256-bit data, 11-bit ID
//                        (harness is the master; lands on the mesh
//                        configuration ingress CFG_AXI — this is the port
//                        the Avocado boot sequencer programs the mesh
//                        through)
//
// DUT <-> mesh connection: DUT chi_rn0..3 (packed CHI-E RN ports) mate the
// mesh RN-F ports RNF32 / RNF64 / RNF12 / RNF44 (LDID order 0..3). Link
// layer is managed by this top: links are held always-up (RXLINKACTIVEREQ=1,
// TXLINKACTIVEACK=TXLINKACTIVEREQ), coherency is held requested
// (SYSCOREQ=1), RXSACTIVE=1, and every RN-F EVENT handshake is looped back
// (EVENTIACK=EVENTIREQ, EVENTOREQ=EVENTOACK). The HN-D downstream peripheral
// master (PERIPH_AXI_*) is parked (ready held low) — nothing sits behind it
// in this integration.
//
// The port contract macros live in integration/avocado_dut_if.svh and are
// shared with the DUT template and the contract checker.
//*****************************************************************************

`include "avocado_dut_if.svh"

module avocado_top (
    input  logic        clock,
    input  logic        reset,

    // 4x Type-1 CCHI ports (to the harness Taurus upstream nodes)
    `AVOCADO_CCHI_T1_PORT_DECL(0),
    `AVOCADO_CCHI_T1_PORT_DECL(1),
    `AVOCADO_CCHI_T1_PORT_DECL(2),
    `AVOCADO_CCHI_T1_PORT_DECL(3),

    // 2x AXI4 memory ports (to the harness memory models)
    `AVOCADO_AXI_M_PORT_DECL(0),
    `AVOCADO_AXI_M_PORT_DECL(1),

    // 1x AXI4 config port (from the harness master / boot sequencer)
    `AVOCADO_AXI_S_PORT_DECL(0)
);

    // ----------------------------------------------------------------------
    // DUT <-> mesh CHI nets (and the per-port loopback wires)
    // ----------------------------------------------------------------------
    `AVOCADO_CHI_RN_WIRES_DECL(0);
    `AVOCADO_CHI_RN_WIRES_DECL(1);
    `AVOCADO_CHI_RN_WIRES_DECL(2);
    `AVOCADO_CHI_RN_WIRES_DECL(3);

    // ----------------------------------------------------------------------
    // User DUT: 4x Type-1 CCHI ports + 4x packed CHI-E RN ports
    // (module name from `AVOCADO_DUT_MODULE, default avocado_dut)
    // ----------------------------------------------------------------------
    `AVOCADO_DUT_MODULE u_dut (
        .clock  (clock),
        .reset  (reset),

        `AVOCADO_CCHI_T1_PORT_CONN(0),
        `AVOCADO_CCHI_T1_PORT_CONN(1),
        `AVOCADO_CCHI_T1_PORT_CONN(2),
        `AVOCADO_CCHI_T1_PORT_CONN(3),

        `AVOCADO_CHI_RN_PORT_CONN(0),
        `AVOCADO_CHI_RN_PORT_CONN(1),
        `AVOCADO_CHI_RN_PORT_CONN(2),
        `AVOCADO_CHI_RN_PORT_CONN(3)
    );

    // ----------------------------------------------------------------------
    // Avocado mesh
    // ----------------------------------------------------------------------
    Avocado u_mesh (
        .CLK    (clock),
        .nRESET (~reset),

        // RN-F ports in LDID order: chi_rn0->RNF32, chi_rn1->RNF64,
        // chi_rn2->RNF12, chi_rn3->RNF44. Link always up, EVENT looped back.
        `AVOCADO_RNF_MESH_CONN(0, 32),
        `AVOCADO_RNF_MESH_CONN(1, 64),
        `AVOCADO_RNF_MESH_CONN(2, 12),
        `AVOCADO_RNF_MESH_CONN(3, 44),

        // Configuration ingress (slave) <- harness AXI master (axi_s0)
        .CFG_AXI_AWID       (axi_s0_awid),
        .CFG_AXI_AWADDR     (axi_s0_awaddr),
        .CFG_AXI_AWLEN      (axi_s0_awlen),
        .CFG_AXI_AWSIZE     (axi_s0_awsize),
        .CFG_AXI_AWBURST    (axi_s0_awburst),
        .CFG_AXI_AWVALID    (axi_s0_awvalid),
        .CFG_AXI_AWREADY    (axi_s0_awready),
        .CFG_AXI_WDATA      (axi_s0_wdata),
        .CFG_AXI_WSTRB      (axi_s0_wstrb),
        .CFG_AXI_WLAST      (axi_s0_wlast),
        .CFG_AXI_WVALID     (axi_s0_wvalid),
        .CFG_AXI_WREADY     (axi_s0_wready),
        .CFG_AXI_BID        (axi_s0_bid),
        .CFG_AXI_BRESP      (axi_s0_bresp),
        .CFG_AXI_BVALID     (axi_s0_bvalid),
        .CFG_AXI_BREADY     (axi_s0_bready),
        .CFG_AXI_ARID       (axi_s0_arid),
        .CFG_AXI_ARADDR     (axi_s0_araddr),
        .CFG_AXI_ARLEN      (axi_s0_arlen),
        .CFG_AXI_ARSIZE     (axi_s0_arsize),
        .CFG_AXI_ARBURST    (axi_s0_arburst),
        .CFG_AXI_ARVALID    (axi_s0_arvalid),
        .CFG_AXI_ARREADY    (axi_s0_arready),
        .CFG_AXI_RID        (axi_s0_rid),
        .CFG_AXI_RDATA      (axi_s0_rdata),
        .CFG_AXI_RRESP      (axi_s0_rresp),
        .CFG_AXI_RLAST      (axi_s0_rlast),
        .CFG_AXI_RVALID     (axi_s0_rvalid),
        .CFG_AXI_RREADY     (axi_s0_rready),

        // HN-D downstream peripheral master: parked (nothing behind it)
        .PERIPH_AXI_AWAKEUP (),
        .PERIPH_AXI_AWID    (),
        .PERIPH_AXI_AWADDR  (),
        .PERIPH_AXI_AWLEN   (),
        .PERIPH_AXI_AWSIZE  (),
        .PERIPH_AXI_AWBURST (),
        .PERIPH_AXI_AWLOCK  (),
        .PERIPH_AXI_AWCACHE (),
        .PERIPH_AXI_AWPROT  (),
        .PERIPH_AXI_AWDOMAIN(),
        .PERIPH_AXI_AWSNOOP (),
        .PERIPH_AXI_AWQOS   (),
        .PERIPH_AXI_AWNSAID (),
        .PERIPH_AXI_AWUSER  (),
        .PERIPH_AXI_AWVALID (),
        .PERIPH_AXI_AWTRACE (),
        .PERIPH_AXI_AWMPAM  (),
        .PERIPH_AXI_AWIDUNQ (),
        .PERIPH_AXI_AWREADY (1'b0),
        .PERIPH_AXI_WDATA   (),
        .PERIPH_AXI_WSTRB   (),
        .PERIPH_AXI_WPOISON (),
        .PERIPH_AXI_WDATACHK(),
        .PERIPH_AXI_WLAST   (),
        .PERIPH_AXI_WUSER   (),
        .PERIPH_AXI_WVALID  (),
        .PERIPH_AXI_WTRACE  (),
        .PERIPH_AXI_WREADY  (1'b0),
        .PERIPH_AXI_BID     ('0),
        .PERIPH_AXI_BRESP   ('0),
        .PERIPH_AXI_BUSER   ('0),
        .PERIPH_AXI_BVALID  (1'b0),
        .PERIPH_AXI_BTRACE  (1'b0),
        .PERIPH_AXI_BIDUNQ  (1'b0),
        .PERIPH_AXI_BREADY  (),
        .PERIPH_AXI_ARID    (),
        .PERIPH_AXI_ARADDR  (),
        .PERIPH_AXI_ARLEN   (),
        .PERIPH_AXI_ARSIZE  (),
        .PERIPH_AXI_ARBURST (),
        .PERIPH_AXI_ARLOCK  (),
        .PERIPH_AXI_ARCACHE (),
        .PERIPH_AXI_ARPROT  (),
        .PERIPH_AXI_ARDOMAIN(),
        .PERIPH_AXI_ARSNOOP (),
        .PERIPH_AXI_ARQOS   (),
        .PERIPH_AXI_ARNSAID (),
        .PERIPH_AXI_ARUSER  (),
        .PERIPH_AXI_ARVALID (),
        .PERIPH_AXI_ARTRACE (),
        .PERIPH_AXI_ARMPAM  (),
        .PERIPH_AXI_ARIDUNQ (),
        .PERIPH_AXI_ARREADY (1'b0),
        .PERIPH_AXI_RID     ('0),
        .PERIPH_AXI_RDATA   ('0),
        .PERIPH_AXI_RPOISON ('0),
        .PERIPH_AXI_RDATACHK('0),
        .PERIPH_AXI_RUSER   (1'b0),
        .PERIPH_AXI_RRESP   ('0),
        .PERIPH_AXI_RLAST   (1'b0),
        .PERIPH_AXI_RVALID  (1'b0),
        .PERIPH_AXI_RTRACE  (1'b0),
        .PERIPH_AXI_RIDUNQ  (1'b0),
        .PERIPH_AXI_RREADY  (),

        // Memory (SBSX) masters -> harness memory models (axi_m0 / axi_m1)
        `AVOCADO_AXI_MEM_CONN(4, 0),
        `AVOCADO_AXI_MEM_CONN(72, 1)
    );

endmodule
