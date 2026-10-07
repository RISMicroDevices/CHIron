//*****************************************************************************
// vcs_smoke.cpp — C++ side of the B0 environment smoke (see vcs_smoke_top.sv).
//
// CohestraVCS_SmokeMain is the imported context task: it captures the caller's
// scope (the vcs_smoke_top instance), calls the exported task
// CohestraVCS_SmokeAdvance to advance 10 clock cycles, and returns the magic
// 42 through the output argument — proving both DPI directions and timed
// exported-task calls work in this VCS environment.
//*****************************************************************************

#include <svdpi.h>

// Exported by vcs_smoke_top (prototype mirrors the export declaration; the
// simulator's generated vc_hdrs.h carries the same signature).
extern "C" void CohestraVCS_SmokeAdvance(int cycles);

extern "C" void CohestraVCS_SmokeMain(int* status)
{
    // This is a context import: svGetScope() is the caller's (vcs_smoke_top)
    // scope; re-enter it explicitly before calling the exported task.
    svScope scope = svGetScope();
    svSetScope(scope);

    // Advance 10 cycles through the exported task (consumes simulation time).
    CohestraVCS_SmokeAdvance(10);

    // The magic round-trip value.
    if (status)
        *status = 42;
}
