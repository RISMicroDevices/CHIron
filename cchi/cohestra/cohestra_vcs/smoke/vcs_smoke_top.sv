//*****************************************************************************
// vcs_smoke_top.sv — B0 environment smoke top for the Cohestra VCS harness.
//
// Exercises the exact DPI mechanics the real bridge relies on, without any
// DUT: a free-running clock, one imported context task round-trip into C++
// (which returns the magic 42), and one exported-task call from C++ that
// advances 10 clock cycles. Prints COHESTRA_VCS_SMOKE_PASS on success.
//
// Optional FSDB sub-probe: compile with +define+VCS_SMOKE_FSDB (and the Verdi
// PLI on the vcs line) to also dump vcs_smoke.fsdb (run_vcs_smoke.sh does this
// as a non-fatal extra stage when VERDI_HOME is set).
//*****************************************************************************

`timescale 1ps/1ps
`default_nettype none

module vcs_smoke_top;

    // Half period in timescale units (1ps): 500 = 500ps, a 1ns (1GHz) clock —
    // the same timescale/period convention as cohestra_vcs_bridge.sv.
    localparam int unsigned HP = 500;

    logic clock;

    // The smoke uses a free-running clock (unlike the real bridge, where C++
    // owns the clock): the exported task below simply waits for its edges.
    initial clock = 1'b0;
    always #(HP) clock = ~clock;

    // Import: the C++ smoke entry. Declared a *task* (not a function) because
    // it calls the time-consuming exported task below — an imported
    // function's call chain is not allowed to block (IEEE 1800 DPI).
    import "DPI-C" context task CohestraVCS_SmokeMain(
        output  int     status
    );

    // Export: called from C++; advances 'cycles' clock cycles.
    export "DPI-C" task CohestraVCS_SmokeAdvance;
    task CohestraVCS_SmokeAdvance(input int cycles);
`ifdef VERILATOR
        // Lint-only path: Verilator (rightly) rejects timing controls inside
        // DPI-exported tasks. The lint flow never simulates.
        clock = ~clock;
`else
        repeat (cycles) @(posedge clock);
`endif
    endtask

    initial begin : smoke_entry
        int              status;
        longint unsigned t0;
        longint unsigned t1;

        status = 0;
        t0 = $time;

        CohestraVCS_SmokeMain(status);

        t1 = $time;

        if (status != 42) begin
            $display("COHESTRA_VCS_SMOKE_FAIL: DPI round-trip returned %0d (expected 42)", status);
            $fatal(1, "smoke: bad round-trip");
        end

        // The 10 posedges of a 0-idle clock land at HP, 3*HP, ..., 19*HP, so
        // from t0=0 the last one is at 10*2*HP - HP (phase offset, not a
        // shortfall).
        if ((t1 - t0) < (10 * 2 * HP - HP)) begin
            $display("COHESTRA_VCS_SMOKE_FAIL: only %0d ps advanced (expected >= %0d ps)",
                t1 - t0, 10 * 2 * HP - HP);
            $fatal(1, "smoke: export task did not advance time");
        end

        $display("COHESTRA_VCS_SMOKE_PASS");
        $finish;
    end

    // Watchdog: if the DPI mechanics wedge the simulation, fail loudly.
    initial begin : smoke_watchdog
        #(1000 * 2 * HP);
        $display("COHESTRA_VCS_SMOKE_FAIL: watchdog timeout (1000 cycles)");
        $fatal(1, "smoke: watchdog timeout");
    end

`ifdef VCS_SMOKE_FSDB
    // Non-fatal FSDB sub-probe (run_vcs_smoke.sh stage FSDB).
    initial begin : smoke_fsdb
        $fsdbDumpfile("vcs_smoke.fsdb");
        $fsdbDumpvars(0, vcs_smoke_top);
    end
`endif

endmodule

`default_nettype wire
