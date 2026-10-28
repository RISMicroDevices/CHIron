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

//*****************************************************************************
// executable.cpp — cohestra_hades entry point: a thin main() over
// Cohestra::Hades::RunHarness (which lives in the cohestra_hades_core static
// library so the HADES rehearsal links the same flow in-process).
//*****************************************************************************
#include "hades_harness.hpp"


int main(int argc, const char* argv[])
{
    return Cohestra::Hades::RunHarness(argc, argv);
}

#endif
