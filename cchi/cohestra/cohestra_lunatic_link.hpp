#pragma once

#include "cohestra_v3/interface_chi.hpp"
#include "cohestra_v3/interface_chi_link.hpp"

#include <array>
#include <functional>
#include <utility>

namespace Cohestra {

    template<class VT, template<class, size_t, class> class ChannelPolicy>
    class LunaticCHIToRNInterfaceT final : public V3CHIToRNInterfaceT<VT, ChannelPolicy> {
    private:
        template<class Flit>
        static bool IsLinkReturn(const Flit& flit) noexcept
        {
            return flit.Opcode() == 0 && flit.TxnID() == 0;
        }

        template<size_t PORT, class ChannelTag, class Flit>
        void DriveChannel(details::CHICreditChannel<Flit>& channel, bool run, bool deactivating) noexcept
        {
            if (run || deactivating)
                channel.template Drive<VT, ChannelPolicy, PORT, ChannelTag>(*this->verilated);
            else
                ChannelPolicy<VT, PORT, ChannelTag>::ClearValid(*this->verilated);
        }

        template<size_t PORT, class ChannelTag, class Flit>
        void ObserveChannel(details::CHICreditChannel<Flit>& channel, bool deactivating) noexcept
        {
            if (channel.rxCredits <= 0)
                return;
            Flit flit{};
            if (!ChannelPolicy<VT, PORT, ChannelTag>::SampleFlit(*this->verilated, static_cast<void*>(&flit)))
                return;
            --channel.rxCredits;
            if (deactivating && IsLinkReturn(flit))
                ++channel.rxPendingReturns;
            else
                channel.rxObservations.push_back(flit);
        }

        std::array<bool, 8> transmitActivated{};

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
                const bool receiveRun = Traits::TxReq(*this->verilated) && Traits::TxAck(*this->verilated); \
                const bool transmitRun = Traits::RxReq(*this->verilated) && Traits::RxAck(*this->verilated); \
                if (transmitRun) \
                    transmitActivated[port] = true; \
                const bool transmitDeactivating = transmitActivated[port] && !transmitRun; \
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
                DriveChannel<port, details::RXRSPChannel>(p.rxrsp, transmitRun, transmitDeactivating); \
                DriveChannel<port, details::RXDATChannel>(p.rxdat, transmitRun, transmitDeactivating); \
                DriveChannel<port, details::RXSNPChannel>(p.rxsnp, transmitRun, transmitDeactivating); \
                const bool creditsReturned = \
                    p.rxrsp.txCredits <= 0 && p.rxrsp.txBuffer.IsEmpty() && !p.rxrsp.txDriven && \
                    p.rxdat.txCredits <= 0 && p.rxdat.txBuffer.IsEmpty() && !p.rxdat.txDriven && \
                    p.rxsnp.txCredits <= 0 && p.rxsnp.txBuffer.IsEmpty() && !p.rxsnp.txDriven; \
                if (transmitActivated[port] && !Traits::RxReq(*this->verilated) && \
                    !Traits::RxAck(*this->verilated) && creditsReturned) \
                    transmitActivated[port] = false; \
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

        void TickPostHandshake(uint64_t time) noexcept override
        {
            if (this->lastPostTickValid && this->lastPostTickTime == time)
                return;
            this->lastPostTickTime = time;
            this->lastPostTickValid = true;

#define _LUNATIC_RETIRE_PORT(port) \
            if constexpr (V3CHIToRNInterfaceT<VT, ChannelPolicy>::HasPort##port()) \
            { \
                auto& p = this->ports[port]; \
                using Traits = details::V3CHILinkPinTraits<VT, port>; \
                p.rxrsp.template Retire<VT, ChannelPolicy, port, details::RXRSPChannel>(*this->verilated); \
                p.rxdat.template Retire<VT, ChannelPolicy, port, details::RXDATChannel>(*this->verilated); \
                p.rxsnp.template Retire<VT, ChannelPolicy, port, details::RXSNPChannel>(*this->verilated); \
                const bool receiveDeactivating = !Traits::TxReq(*this->verilated) && Traits::TxAck(*this->verilated); \
                ObserveChannel<port, details::TXREQChannel>(p.txreq, receiveDeactivating); \
                ObserveChannel<port, details::TXRSPChannel>(p.txrsp, receiveDeactivating); \
                ObserveChannel<port, details::TXDATChannel>(p.txdat, receiveDeactivating); \
            }
            _LUNATIC_RETIRE_PORT(0)
            _LUNATIC_RETIRE_PORT(1)
            _LUNATIC_RETIRE_PORT(2)
            _LUNATIC_RETIRE_PORT(3)
            _LUNATIC_RETIRE_PORT(4)
            _LUNATIC_RETIRE_PORT(5)
            _LUNATIC_RETIRE_PORT(6)
            _LUNATIC_RETIRE_PORT(7)
#undef _LUNATIC_RETIRE_PORT
        }
    };

    template<class VT, template<class, size_t, class> class ChannelPolicy>
    class LunaticCHILinkControlT {
    public:
        using Transport = LunaticCHIToRNInterfaceT<VT, ChannelPolicy>;

    private:
        VT*                         verilated;
        Transport*                  transport;
        bool                        active = false;
        std::array<bool, 8>         transmitStopped{};
        std::array<bool, 8>         remoteTransmitRun{};
        std::function<bool()>       protocolDrained;

        bool PortProtocolDrained(size_t port) const noexcept
        {
            if (protocolDrained && !protocolDrained())
                return false;
            for (const auto& status : transport->GetPortStatus(port))
                if (status.txQueue || status.rxQueue || status.txDriven)
                    return false;
            return true;
        }

        bool ReceiveCreditsReturned(size_t port) const noexcept
        {
            const auto status = transport->GetPortStatus(port);
            for (size_t index = 3; index < status.size(); ++index)
                if (status[index].rxQueue || status[index].rxCredits)
                    return false;
            return true;
        }

        bool TransmitCreditsReturned(size_t port) const noexcept
        {
            const auto status = transport->GetPortStatus(port);
            for (size_t index = 0; index < 3; ++index)
                if (status[index].txCredits || status[index].txQueue || status[index].txDriven)
                    return false;
            return true;
        }

        void QueueCreditReturns(size_t port) noexcept
        {
            const auto status = transport->GetPortStatus(port);
            if (status[0].txCredits > 0 && !status[0].txQueue && !status[0].txDriven)
                transport->PushRXRSP(port, CHI::RSPFlit{});
            if (status[1].txCredits > 0 && !status[1].txQueue && !status[1].txDriven)
                transport->PushRXDAT(port, CHI::DATFlit{});
            if (status[2].txCredits > 0 && !status[2].txQueue && !status[2].txDriven)
                transport->PushRXSNP(port, CHI::SNPFlit{});
        }

    public:
        static constexpr bool HasPort0() noexcept { return Transport::HasPort0(); }
        static constexpr bool HasPort1() noexcept { return Transport::HasPort1(); }
        static constexpr bool HasPort2() noexcept { return Transport::HasPort2(); }
        static constexpr bool HasPort3() noexcept { return Transport::HasPort3(); }
        static constexpr bool HasPort4() noexcept { return Transport::HasPort4(); }
        static constexpr bool HasPort5() noexcept { return Transport::HasPort5(); }
        static constexpr bool HasPort6() noexcept { return Transport::HasPort6(); }
        static constexpr bool HasPort7() noexcept { return Transport::HasPort7(); }

        static constexpr bool HasPort(size_t index) noexcept
        {
            switch (index)
            {
                case 0: return HasPort0();
                case 1: return HasPort1();
                case 2: return HasPort2();
                case 3: return HasPort3();
                case 4: return HasPort4();
                case 5: return HasPort5();
                case 6: return HasPort6();
                case 7: return HasPort7();
                default: return false;
            }
        }

        LunaticCHILinkControlT(VT* verilated, Transport* transport) noexcept
            : verilated(verilated), transport(transport)
        {
            transmitStopped.fill(true);
        }

        void SetProtocolDrained(std::function<bool()> callback) noexcept
        {
            protocolDrained = std::move(callback);
        }

        void Reset() noexcept
        {
            active = false;
            transmitStopped.fill(true);
            remoteTransmitRun.fill(false);
            TickPreHandshake();
        }

        void SetActive(bool value) noexcept
        {
            active = value;
            if (active)
            {
                transmitStopped.fill(false);
                remoteTransmitRun.fill(false);
            }
        }

        void TickPreHandshake() noexcept
        {
#define _LUNATIC_TICK_LINK(port) \
            if constexpr (HasPort##port()) \
            { \
                using Traits = details::V3CHILinkPinTraits<VT, port>; \
                const bool transmitRun = Traits::RxReq(*verilated) && Traits::RxAck(*verilated); \
                if (transmitRun) \
                    remoteTransmitRun[port] = true; \
                if (active && Traits::RxAck(*verilated)) \
                    transmitStopped[port] = false; \
                else if (!transmitStopped[port] && PortProtocolDrained(port) && \
                    (!active || remoteTransmitRun[port])) \
                    transmitStopped[port] = true; \
                Traits::RxReq(*verilated) = transmitStopped[port] ? 0 : 1; \
                bool acknowledge = Traits::TxAck(*verilated) != 0; \
                if (Traits::TxReq(*verilated)) \
                { \
                    if (active || acknowledge) \
                        acknowledge = true; \
                } \
                else if (acknowledge && ReceiveCreditsReturned(port)) \
                    acknowledge = false; \
                Traits::TxAck(*verilated) = acknowledge ? 1 : 0; \
                Traits::RxSActive(*verilated) = Traits::RxReq(*verilated) || acknowledge ? 1 : 0; \
                Traits::SyscoAck(*verilated) = active ? 1 : 0; \
                if (transmitStopped[port]) \
                    QueueCreditReturns(port); \
                if (!Traits::RxReq(*verilated) && !Traits::RxAck(*verilated) && TransmitCreditsReturned(port)) \
                    remoteTransmitRun[port] = false; \
            }
            _LUNATIC_TICK_LINK(0)
            _LUNATIC_TICK_LINK(1)
            _LUNATIC_TICK_LINK(2)
            _LUNATIC_TICK_LINK(3)
            _LUNATIC_TICK_LINK(4)
            _LUNATIC_TICK_LINK(5)
            _LUNATIC_TICK_LINK(6)
            _LUNATIC_TICK_LINK(7)
#undef _LUNATIC_TICK_LINK
        }

        bool IsRun(size_t index) const noexcept
        {
#define _LUNATIC_IS_RUN(port) \
            if (index == port) \
            { \
                if constexpr (HasPort##port()) \
                { \
                    using Traits = details::V3CHILinkPinTraits<VT, port>; \
                    return active && Traits::RxReq(*verilated) && Traits::RxAck(*verilated) && \
                        Traits::TxReq(*verilated) && Traits::TxAck(*verilated); \
                } \
                else \
                    return false; \
            }
            _LUNATIC_IS_RUN(0)
            _LUNATIC_IS_RUN(1)
            _LUNATIC_IS_RUN(2)
            _LUNATIC_IS_RUN(3)
            _LUNATIC_IS_RUN(4)
            _LUNATIC_IS_RUN(5)
            _LUNATIC_IS_RUN(6)
            _LUNATIC_IS_RUN(7)
#undef _LUNATIC_IS_RUN
            return false;
        }
    };

    template<class VT>
    using LunaticCHIToRNInterface = LunaticCHIToRNInterfaceT<VT, details::V3CHISeparateFieldRNChannelPolicy>;

    template<class VT>
    using LunaticCHILinkControl = LunaticCHILinkControlT<VT, details::V3CHISeparateFieldRNChannelPolicy>;

}
