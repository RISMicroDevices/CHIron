#pragma once

#ifndef __CCHI__CCHI_ICN_TAURUS__COMPONENT_AFX
#define __CCHI__CCHI_ICN_TAURUS__COMPONENT_AFX

#include "../../spec/cchi_protocol_flits.hpp"           // IWYU pragma: keep


namespace CCHI::Taurus {

    template<class TEvent>
    class FutureNow;

    template<FlitConfigurationConcept config>
    class UpstreamNode;
}

#endif // __CCHI__CCHI_ICN_TAURUS__COMPONENT_AFX
