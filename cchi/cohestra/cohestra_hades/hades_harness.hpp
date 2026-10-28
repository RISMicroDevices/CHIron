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

#if defined(COHESTRA_HADES)
#  if defined(__has_include) && !__has_include(<hades/transport.hpp>)
#    error "cohestra_hades: COHESTRA_HADES defined but HADES-Fabric headers not on the include path (set HADES_FABRIC_ROOT)"
#  endif
#endif

// Self-neutralizing guard: outside this layer's own opt-in build the whole
// file compiles to an empty translation unit. Content is enabled either by
// the opt-in macro (cohestra_hades build, consumers linking the CMake target)
// or simply by having the HADES-Fabric headers on the include path (the
// HADES rehearsal, which links the archive by file path and therefore does
// not inherit the target's PUBLIC defines).
#if defined(COHESTRA_HADES) || __has_include(<hades/transport.hpp>)

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

#endif
