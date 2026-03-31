/*
 * CHIron VIP — Example SystemVerilog Monitor Module
 *
 * This module passively monitors all CHI channels for a single Requester
 * Node (RN-F) and reports protocol violations discovered by CHIron's
 * transaction model and cache-state tracker.
 *
 * Instantiation example:
 *
 *   chi_xact_monitor #(
 *       .NODE_ID     (7'h04),
 *       .HN_NODE_ID  (7'h40),
 *       .NODE_TYPE   (`CHIRONVIP_NODE_TYPE_RN_F),
 *       .ENABLE_STATE_CHECK (1)
 *   ) u_mon (
 *       .clk          (clk),
 *       .txreq_valid  (rnf_txreq_valid),
 *       .txreq_flit   (rnf_txreq_flit),
 *       .rxrsp_valid  (rnf_rxrsp_valid),
 *       .rxrsp_flit   (rnf_rxrsp_flit),
 *       .rxdat_valid  (rnf_rxdat_valid),
 *       .rxdat_flit   (rnf_rxdat_flit),
 *       .rxsnp_valid  (rnf_rxsnp_valid),
 *       .rxsnp_flit   (rnf_rxsnp_flit),
 *       .txrsp_valid  (rnf_txrsp_valid),
 *       .txrsp_flit   (rnf_txrsp_flit),
 *       .txdat_valid  (rnf_txdat_valid),
 *       .txdat_flit   (rnf_txdat_flit),
 *       .violation    (rnf_mon_violation)
 *   );
 *
 * Flit vectors must be right-justified, zero-padded to 512 bits.
 * Only the lowest CHIRONVIP_*_FLIT_BITS bits are significant.
 */

`include "chi_xact_vip_dpi.svh"

module chi_xact_monitor
#(
    // Node ID of the RN-F node being monitored.
    parameter int unsigned  NODE_ID               = 0,

    // Node ID of the Home Node this RN-F is connected to.
    parameter int unsigned  HN_NODE_ID            = 0,

    // CHI node type for NODE_ID.  Use CHIRONVIP_NODE_TYPE_RN_F for a
    // fully-coherent requester.
    parameter int           NODE_TYPE             = `CHIRONVIP_NODE_TYPE_RN_F,

    // Also instantiate a cache-state tracker and check coherence
    // state transitions against the protocol.
    parameter bit           ENABLE_STATE_CHECK    = 1'b1,

    // Silent transition flags forwarded to the cache-state tracker.
    parameter bit           SILENT_EVICTION       = 1'b0,
    parameter bit           SILENT_SHARING        = 1'b0,
    parameter bit           SILENT_STORE          = 1'b0,
    parameter bit           SILENT_INVALIDATION   = 1'b0,

    // Disable strict field-mapping checks (useful during bring-up).
    parameter bit           FIELD_CHECK_ENABLE    = 1'b1,

    // Flit widths (bits).  Must match the compiled-in DPI library config.
    parameter int           REQ_FLIT_BITS         = `CHIRONVIP_REQ_FLIT_BITS,
    parameter int           SNP_FLIT_BITS         = `CHIRONVIP_SNP_FLIT_BITS,
    parameter int           RSP_FLIT_BITS         = `CHIRONVIP_RSP_FLIT_BITS,
    parameter int           DAT_FLIT_BITS         = `CHIRONVIP_DAT_FLIT_BITS
)
(
    input  logic            clk,

    // TXREQ channel (RN-F → HN-F)
    input  logic            txreq_valid,
    input  bit [511:0]      txreq_flit,

    // RXRSP channel (HN-F → RN-F)
    input  logic            rxrsp_valid,
    input  bit [511:0]      rxrsp_flit,

    // RXDAT channel (HN-F/SN-F → RN-F)
    input  logic            rxdat_valid,
    input  bit [511:0]      rxdat_flit,

    // RXSNP channel (HN-F → RN-F)
    input  logic            rxsnp_valid,
    input  bit [511:0]      rxsnp_flit,

    // TXRSP channel (RN-F → HN-F, snoop responses)
    input  logic            txrsp_valid,
    input  bit [511:0]      txrsp_flit,

    // TXDAT channel (RN-F → HN-F, copy-back / snoop data)
    input  logic            txdat_valid,
    input  bit [511:0]      txdat_flit,

    // Violation indicator: pulses high for one cycle when CHIron
    // detects a protocol error on any channel.
    output logic            violation
);

    // -----------------------------------------------------------------------
    // DPI handles
    // -----------------------------------------------------------------------
    chandle rnf_joint;
    chandle global_ctx;
    chandle state_map;

    // -----------------------------------------------------------------------
    // Initialisation
    // -----------------------------------------------------------------------
    initial begin : init_chiron
        // Create the global context and register topology nodes
        global_ctx = CHIronVIP_CreateGlobal();
        CHIronVIP_Global_AddTopoNode(global_ctx, NODE_ID,   NODE_TYPE,               "rnf");
        CHIronVIP_Global_AddTopoNode(global_ctx, HN_NODE_ID, `CHIRONVIP_NODE_TYPE_HN_F, "hnf");
        CHIronVIP_Global_SetFieldCheckEnable(global_ctx, FIELD_CHECK_ENABLE ? 1 : 0);

        // Create the RNF joint (transaction model)
        rnf_joint = CHIronVIP_CreateRNFJoint();

        // Optionally create a cache-state tracker
        if (ENABLE_STATE_CHECK) begin
            state_map = CHIronVIP_CreateRNCacheStateMap();
            CHIronVIP_StateMap_SetSilentEviction    (state_map, SILENT_EVICTION     ? 1 : 0);
            CHIronVIP_StateMap_SetSilentSharing     (state_map, SILENT_SHARING      ? 1 : 0);
            CHIronVIP_StateMap_SetSilentStore       (state_map, SILENT_STORE        ? 1 : 0);
            CHIronVIP_StateMap_SetSilentInvalidation(state_map, SILENT_INVALIDATION ? 1 : 0);
        end else begin
            state_map = null;
        end
    end

    // -----------------------------------------------------------------------
    // Cleanup on end-of-simulation
    // -----------------------------------------------------------------------
    final begin : fini_chiron
        if (rnf_joint  != null) CHIronVIP_DestroyRNFJoint(rnf_joint);
        if (state_map  != null) CHIronVIP_DestroyRNCacheStateMap(state_map);
        if (global_ctx != null) CHIronVIP_DestroyGlobal(global_ctx);
    end

    // -----------------------------------------------------------------------
    // Per-cycle flit processing
    // -----------------------------------------------------------------------
    always_ff @(posedge clk) begin : monitor_flits
        int     denial;
        longint txnid_addr;     // address extracted from TXREQ for state-map calls

        violation <= 1'b0;

        // --- TXREQ ----------------------------------------------------------
        if (txreq_valid) begin
            denial = CHIronVIP_RNF_NextTXREQ(rnf_joint, global_ctx,
                                              $time, txreq_flit, REQ_FLIT_BITS);
            if (denial !== `CHIRONVIP_ACCEPTED) begin
                $error("[CHIron][%0t] TXREQ violation at node 0x%0x: %s",
                       $time, NODE_ID, CHIronVIP_GetDenialName(denial));
                violation <= 1'b1;
            end

            // Cache-state check: extract REQ.Addr from the flit.
            // For default config (7-bit NodeID, 48-bit addr): bits [107:60].
            if (ENABLE_STATE_CHECK && state_map !== null) begin
                txnid_addr = txreq_flit[107:60];
                denial = CHIronVIP_StateMap_NextTXREQ(state_map, txnid_addr,
                                                       txreq_flit, REQ_FLIT_BITS);
                if (denial !== `CHIRONVIP_ACCEPTED) begin
                    $error("[CHIron][%0t] TXREQ cache-state violation at node 0x%0x: %s",
                           $time, NODE_ID, CHIronVIP_GetDenialName(denial));
                    violation <= 1'b1;
                end
            end
        end

        // --- RXSNP ----------------------------------------------------------
        if (rxsnp_valid) begin
            denial = CHIronVIP_RNF_NextRXSNP(rnf_joint, global_ctx,
                                              $time, rxsnp_flit, SNP_FLIT_BITS);
            if (denial !== `CHIRONVIP_ACCEPTED) begin
                $error("[CHIron][%0t] RXSNP violation at node 0x%0x: %s",
                       $time, NODE_ID, CHIronVIP_GetDenialName(denial));
                violation <= 1'b1;
            end

            if (ENABLE_STATE_CHECK && state_map !== null) begin
                // SNP.Addr is snpAddrWidth = reqAddrWidth-3 bits wide.
                // For default config (7-bit NodeID, 48-bit addr): SNP.Addr is bits [91:47].
                // The snoop address is left-shifted by 3 to recover full address.
                longint snp_addr;
                snp_addr = {rxsnp_flit[91:47], 3'b000};
                denial = CHIronVIP_StateMap_NextRXSNP(state_map, snp_addr,
                                                       rxsnp_flit, SNP_FLIT_BITS);
                if (denial !== `CHIRONVIP_ACCEPTED) begin
                    $error("[CHIron][%0t] RXSNP cache-state violation at node 0x%0x: %s",
                           $time, NODE_ID, CHIronVIP_GetDenialName(denial));
                    violation <= 1'b1;
                end
            end
        end

        // --- TXRSP ----------------------------------------------------------
        if (txrsp_valid) begin
            denial = CHIronVIP_RNF_NextTXRSP(rnf_joint, global_ctx,
                                              $time, txrsp_flit, RSP_FLIT_BITS);
            if (denial !== `CHIRONVIP_ACCEPTED) begin
                $error("[CHIron][%0t] TXRSP violation at node 0x%0x: %s",
                       $time, NODE_ID, CHIronVIP_GetDenialName(denial));
                violation <= 1'b1;
            end
        end

        // --- RXRSP ----------------------------------------------------------
        if (rxrsp_valid) begin
            denial = CHIronVIP_RNF_NextRXRSP(rnf_joint, global_ctx,
                                              $time, rxrsp_flit, RSP_FLIT_BITS);
            if (denial !== `CHIRONVIP_ACCEPTED) begin
                $error("[CHIron][%0t] RXRSP violation at node 0x%0x: %s",
                       $time, NODE_ID, CHIronVIP_GetDenialName(denial));
                violation <= 1'b1;
            end
        end

        // --- TXDAT ----------------------------------------------------------
        if (txdat_valid) begin
            denial = CHIronVIP_RNF_NextTXDAT(rnf_joint, global_ctx,
                                              $time, txdat_flit, DAT_FLIT_BITS);
            if (denial !== `CHIRONVIP_ACCEPTED) begin
                $error("[CHIron][%0t] TXDAT violation at node 0x%0x: %s",
                       $time, NODE_ID, CHIronVIP_GetDenialName(denial));
                violation <= 1'b1;
            end
        end

        // --- RXDAT ----------------------------------------------------------
        if (rxdat_valid) begin
            denial = CHIronVIP_RNF_NextRXDAT(rnf_joint, global_ctx,
                                              $time, rxdat_flit, DAT_FLIT_BITS);
            if (denial !== `CHIRONVIP_ACCEPTED) begin
                $error("[CHIron][%0t] RXDAT violation at node 0x%0x: %s",
                       $time, NODE_ID, CHIronVIP_GetDenialName(denial));
                violation <= 1'b1;
            end
        end
    end

    // -----------------------------------------------------------------------
    // Optional: periodic in-flight transaction count report
    // -----------------------------------------------------------------------
`ifdef CHIRONVIP_VERBOSE
    always @(posedge clk) begin : report_inflight
        static int prev_count = 0;
        int count;
        count = CHIronVIP_RNF_GetInflightCount(rnf_joint, global_ctx);
        if (count !== prev_count) begin
            $display("[CHIron][%0t] node 0x%0x in-flight count: %0d → %0d",
                     $time, NODE_ID, prev_count, count);
            prev_count = count;
        end
    end
`endif

endmodule : chi_xact_monitor
