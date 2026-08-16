#pragma once

#ifndef __CCHI__CCHI_ICN_TAURUS__SAM
#define __CCHI__CCHI_ICN_TAURUS__SAM

#include "../../spec/cchi_protocol_flits.hpp"


namespace CCHI::Taurus {
    //
    template<FlitConfigurationConcept config>
    class SAM {
    public:
        virtual ~SAM() noexcept = default;

    public:
        virtual Flits::dn_nodeid_t<config> Map(typename Flits::REQ<config>::addr_t PA) const noexcept = 0;
    };

    template<FlitConfigurationConcept config>
    class NoSAM : public SAM<config> {
    public:
        virtual Flits::dn_nodeid_t<config> Map(typename Flits::REQ<config>::addr_t PA) const noexcept override;
    };
}


// Implementation of: class NoSAM
namespace CCHI::Taurus {

    template<FlitConfigurationConcept config>
    inline Flits::dn_nodeid_t<config> NoSAM<config>::Map(typename Flits::REQ<config>::addr_t PA) const noexcept
    {
        return 0;
    }
}


#endif // __CCHI__CCHI_ICN_TAURUS__SAM
