#pragma once

#include "cohestra_v3/interface_chi.hpp"
#include "cohestra_v3/interface_chi_link.hpp"

namespace Cohestra {

    template<class VT, template<class, size_t, class> class ChannelPolicy>
    class LunaticCHIToRNInterfaceT final : public V3CHIToRNInterfaceT<VT, ChannelPolicy> {
    public:
        explicit LunaticCHIToRNInterfaceT(VT* verilated) noexcept
            : V3CHIToRNInterfaceT<VT, ChannelPolicy>(verilated)
        {
        }

        void TickPreHandshake(uint64_t time) noexcept override
        {
            if (this->lastPreTickValid && this->lastPreTickTime == time)
                return;
            this->lastPreTickTime = time;
            this->lastPreTickValid = true;

#define _LUNATIC_TICK_PORT(port) \
            if constexpr (V3CHIToRNInterfaceT<VT, ChannelPolicy>::HasPort##port()) \
            { \
                auto& p = this->ports[port]; \
                using Traits = details::V3CHILinkPinTraits<VT, port>; \
                const bool receiveRun = Traits::RxReq(*this->verilated) && Traits::TxAck(*this->verilated); \
                const bool transmitRun = Traits::TxReq(*this->verilated) && Traits::RxAck(*this->verilated); \
                if (receiveRun) \
                { \
                    p.txreq.template ReturnCredits<VT, ChannelPolicy, port, details::TXREQChannel>(*this->verilated); \
                    p.txrsp.template ReturnCredits<VT, ChannelPolicy, port, details::TXRSPChannel>(*this->verilated); \
                    p.txdat.template ReturnCredits<VT, ChannelPolicy, port, details::TXDATChannel>(*this->verilated); \
                } \
                else \
                { \
                    ChannelPolicy<VT, port, details::TXREQChannel>::ClearLCrdV(*this->verilated); \
                    ChannelPolicy<VT, port, details::TXRSPChannel>::ClearLCrdV(*this->verilated); \
                    ChannelPolicy<VT, port, details::TXDATChannel>::ClearLCrdV(*this->verilated); \
                } \
                if (transmitRun) \
                { \
                    p.rxrsp.template Drive<VT, ChannelPolicy, port, details::RXRSPChannel>(*this->verilated); \
                    p.rxdat.template Drive<VT, ChannelPolicy, port, details::RXDATChannel>(*this->verilated); \
                    p.rxsnp.template Drive<VT, ChannelPolicy, port, details::RXSNPChannel>(*this->verilated); \
                } \
                else \
                { \
                    ChannelPolicy<VT, port, details::RXRSPChannel>::ClearValid(*this->verilated); \
                    ChannelPolicy<VT, port, details::RXDATChannel>::ClearValid(*this->verilated); \
                    ChannelPolicy<VT, port, details::RXSNPChannel>::ClearValid(*this->verilated); \
                } \
            }
            _LUNATIC_TICK_PORT(0)
            _LUNATIC_TICK_PORT(1)
            _LUNATIC_TICK_PORT(2)
            _LUNATIC_TICK_PORT(3)
            _LUNATIC_TICK_PORT(4)
            _LUNATIC_TICK_PORT(5)
            _LUNATIC_TICK_PORT(6)
            _LUNATIC_TICK_PORT(7)
#undef _LUNATIC_TICK_PORT
        }
    };

    template<class VT>
    using LunaticCHIToRNInterface = LunaticCHIToRNInterfaceT<VT, details::V3CHISeparateFieldRNChannelPolicy>;

}
