//
// avocado_script_sequencer.cpp — explicit instantiation of the Avocado
// script sequencer for Cohestra::AXIMaster.
//
#include "avocado_script_sequencer.hpp"

#include "../../../cohestra_axi.hpp"


template class Cohestra::AvocadoScriptSequencer<Cohestra::AXIMaster>;
