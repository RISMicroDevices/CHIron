#pragma once

#ifndef CHI_ISSUE_EB_ENABLE
#define CHI_ISSUE_EB_ENABLE
#endif

#include "chiron_eb_expresso.hpp"

namespace CHIron::Gui {

using ViewerConfig = CHI::Eb::FlitConfiguration<
    11,
    48,
    0,
    0,
    256,
    false,
    false,
    true>;

using ReqFlit = CHI::Eb::Flits::REQ<ViewerConfig>;
using RspFlit = CHI::Eb::Flits::RSP<ViewerConfig>;
using DatFlit = CHI::Eb::Flits::DAT<ViewerConfig>;
using SnpFlit = CHI::Eb::Flits::SNP<ViewerConfig>;

using FieldKey = CHI::Eb::Expresso::Flit::Key;
using FieldKeyValueMap = CHI::Eb::Expresso::Flit::KeyValueMap;

}  // namespace CHIron::Gui
