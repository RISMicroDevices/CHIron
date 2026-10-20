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
