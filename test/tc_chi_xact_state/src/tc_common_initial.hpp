#pragma once

#include "tc_common.hpp"            // IWYU pragma: keep


class TCPtInitial : public TCPt<TCPtInitial> {
private:
    Flits::REQ<config>::addr_t              addr        = 0;
    Flits::REQ<config>                      req         = { 0 };
    std::shared_ptr<Xact::Xaction<config>>  xaction;
    Xact::RNFJoint<config>                  rnJoint;
    Xact::RNCacheStateMap<config>           rnStates;

public:
    virtual ~TCPtInitial() noexcept = default;

    TCPtInitial* ForkREQ(std::string title, Flits::REQ<config>::opcode_t opcode) noexcept;
    
    TCPtInitial* ForkSilentTransitions(
        std::string     title,
        bool            enableSilentEviction,
        bool            enableSilentSharing,
        bool            enableSilentStore,
        bool            enableSilentInvalidation
    ) noexcept;

    TCPtInitial* Leaf(std::string title, Xact::CacheState state, bool accept) noexcept;
};
