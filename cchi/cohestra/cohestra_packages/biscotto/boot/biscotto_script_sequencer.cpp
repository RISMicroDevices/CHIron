//
// biscotto_script_sequencer.cpp — explicit instantiation of the Biscotto
// script sequencer for Cohestra::AXIMaster.
//
#include "biscotto_script_sequencer.hpp"

#include "../../../cohestra_axi.hpp"


template class Cohestra::BiscottoScriptSequencer<Cohestra::AXIMaster>;
