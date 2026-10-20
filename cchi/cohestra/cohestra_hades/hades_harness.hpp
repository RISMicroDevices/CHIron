//*****************************************************************************
// hades_harness.hpp — harness entry of the cohestra_hades layer.
//
// RunHarness is the whole v3-shaped flow (INI config, Instance + stimulators
// + monitors/watchdog/profiler, AXI memory, reset stage, run loop, verdict)
// bound to a HADES-Fabric link instead of a Verilated model. It lives in the
// cohestra_hades_core static library so the HADES rehearsal
// (demo/oceanus_l2/scripts/run_rehearsal.sh -> ctest oceanus_rehearsal) can
// drive the same flow in-process; executable.cpp is a thin main() over it.
//*****************************************************************************
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "../cohestra_v3/arguments.hpp"

#include "hades_link.hpp"


namespace Cohestra::Hades {

    // CLI: v3-compatible (-c/--config, -n/--cycles, --log-file[=*],
    // --no-log-file, -h/--help, --) plus:
    //   --seed <n>        override [stimulator.N] fuzz.ari.seed (run_hardware.sh)
    //   --backend <name>  override [hades] backend (e.g. loopback for rehearsal)
    struct HadesArgumentsContext : public ArgumentsContext {
        std::optional<uint64_t> seedOverride;
        std::optional<std::string> backendOverride;
    };

    HadesArgumentsContext ParseHadesArguments(int argc, const char* argv[]) noexcept;

    // The full harness. Exit codes match cohestra_v3: 0 = FINISHED clean,
    // 1 = setup/config error, 2 = simulation FAILED (or monitor mismatch).
    int RunHarness(int argc, const char* argv[]) noexcept;
}
