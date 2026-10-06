#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "cohestra_config.hpp"

#include "spdlog/spdlog.h"

#include "../../common/eventbus.hpp"

#include "../../chi_eb/spec/chi_eb_protocol.hpp"


namespace Cohestra::CHI {

    // AMBA CHI Issue E.b flit configuration used by the Cohestra interface
    // layer. Defaults match the reference DUT geometry; override per-DUT from
    // the build system (-DCOHESTRA_CHI_*), e.g. for the XSCache Oceanus L2:
    // nodeIdWidth=11 with DataCheck/Poison/MPAM present.
#ifndef COHESTRA_CHI_NODEID_WIDTH
#define COHESTRA_CHI_NODEID_WIDTH           7
#endif
#ifndef COHESTRA_CHI_REQ_ADDR_WIDTH
#define COHESTRA_CHI_REQ_ADDR_WIDTH         48
#endif
#ifndef COHESTRA_CHI_REQ_RSVDC_WIDTH
#define COHESTRA_CHI_REQ_RSVDC_WIDTH        4
#endif
#ifndef COHESTRA_CHI_DAT_RSVDC_WIDTH
#define COHESTRA_CHI_DAT_RSVDC_WIDTH        4
#endif
#ifndef COHESTRA_CHI_DATA_WIDTH
#define COHESTRA_CHI_DATA_WIDTH             256
#endif
#ifndef COHESTRA_CHI_DATACHECK_PRESENT
#define COHESTRA_CHI_DATACHECK_PRESENT      0
#endif
#ifndef COHESTRA_CHI_POISON_PRESENT
#define COHESTRA_CHI_POISON_PRESENT         0
#endif
#ifndef COHESTRA_CHI_MPAM_PRESENT
#define COHESTRA_CHI_MPAM_PRESENT           0
#endif

    using CommonFlitConfiguration = ::CHI::Eb::FlitConfiguration<
            COHESTRA_CHI_NODEID_WIDTH,
            COHESTRA_CHI_REQ_ADDR_WIDTH,
            COHESTRA_CHI_REQ_RSVDC_WIDTH,
            COHESTRA_CHI_DAT_RSVDC_WIDTH,
            COHESTRA_CHI_DATA_WIDTH,
            COHESTRA_CHI_DATACHECK_PRESENT != 0,
            COHESTRA_CHI_POISON_PRESENT != 0,
            COHESTRA_CHI_MPAM_PRESENT != 0
        >;

    using REQFlit = ::CHI::Eb::Flits::REQ<CommonFlitConfiguration>;
    using RSPFlit = ::CHI::Eb::Flits::RSP<CommonFlitConfiguration>;
    using DATFlit = ::CHI::Eb::Flits::DAT<CommonFlitConfiguration>;
    using SNPFlit = ::CHI::Eb::Flits::SNP<CommonFlitConfiguration>;


    // CHI link-layer bundle for a single channel. Unlike AXI valid/ready,
    // AMBA CHI uses L-Credit flow control: a flit is transferred when the
    // transmitter asserts FLITV and an L-Credit is available at the receiver.
    // The receiver returns credits by asserting LCRDV.
    template<class TFlit>
    class BundleChannel {
    public:
        uint8_t     FlitPend    = 0;    // early indication of a following flit
        uint8_t     FlitV       = 0;    // transmitter: flit is valid
        uint8_t     LCrdV       = 0;    // receiver: returning an L-Credit
        TFlit       Flit;

    public:
        bool        Fire() const noexcept { return FlitV != 0 && LCrdV != 0; }
    };

    using BundleChannelREQ  = BundleChannel<REQFlit>;
    using BundleChannelRSP  = BundleChannel<RSPFlit>;
    using BundleChannelDAT  = BundleChannel<DATFlit>;
    using BundleChannelSNP  = BundleChannel<SNPFlit>;


    // A timestamped flit captured at a completed handshake.
    template<class TFlit>
    class FiredFlit {
    public:
        uint64_t    Time    = 0;
        TFlit       Flit;
    };

    using FiredREQ  = FiredFlit<REQFlit>;
    using FiredRSP  = FiredFlit<RSPFlit>;
    using FiredDAT  = FiredFlit<DATFlit>;
    using FiredSNP  = FiredFlit<SNPFlit>;


    // Base class for CHI-related events.
    class EventBase {
    protected:
        uint64_t    time;

    public:
        EventBase(uint64_t time) noexcept;
        uint64_t    GetTime() const noexcept;
    };


    // Per-channel accept events, fired when a handshake completes on a
    // monitored channel.
    class TXREQAcceptEvent : public EventBase
                           , public Gravity::Event<TXREQAcceptEvent> {
    protected:
        FiredREQ    fired;
    public:
        TXREQAcceptEvent(uint64_t time, const FiredREQ& fired) noexcept;
        const FiredREQ& GetFired() const noexcept;
    };

    class TXRSPAcceptEvent : public EventBase
                           , public Gravity::Event<TXRSPAcceptEvent> {
    protected:
        FiredRSP    fired;
    public:
        TXRSPAcceptEvent(uint64_t time, const FiredRSP& fired) noexcept;
        const FiredRSP& GetFired() const noexcept;
    };

    class TXDATAcceptEvent : public EventBase
                           , public Gravity::Event<TXDATAcceptEvent> {
    protected:
        FiredDAT    fired;
    public:
        TXDATAcceptEvent(uint64_t time, const FiredDAT& fired) noexcept;
        const FiredDAT& GetFired() const noexcept;
    };

    class RXRSPAcceptEvent : public EventBase
                           , public Gravity::Event<RXRSPAcceptEvent> {
    protected:
        FiredRSP    fired;
    public:
        RXRSPAcceptEvent(uint64_t time, const FiredRSP& fired) noexcept;
        const FiredRSP& GetFired() const noexcept;
    };

    class RXDATAcceptEvent : public EventBase
                           , public Gravity::Event<RXDATAcceptEvent> {
    protected:
        FiredDAT    fired;
    public:
        RXDATAcceptEvent(uint64_t time, const FiredDAT& fired) noexcept;
        const FiredDAT& GetFired() const noexcept;
    };

    class RXSNPAcceptEvent : public EventBase
                           , public Gravity::Event<RXSNPAcceptEvent> {
    protected:
        FiredSNP    fired;
    public:
        RXSNPAcceptEvent(uint64_t time, const FiredSNP& fired) noexcept;
        const FiredSNP& GetFired() const noexcept;
    };

    class RXREQAcceptEvent : public EventBase
                           , public Gravity::Event<RXREQAcceptEvent> {
    protected:
        FiredREQ    fired;
    public:
        RXREQAcceptEvent(uint64_t time, const FiredREQ& fired) noexcept;
        const FiredREQ& GetFired() const noexcept;
    };


    // CHIFromRNInterface: the agent acts as the RN, driving requests,
    // responses and data towards the DUT and observing inbound responses,
    // data and snoops.
    //
    // Tick contract (same as AXI/CCHI interfaces):
    //   TickPreHandshake  -> clock=0 eval -> TickPostHandshake -> clock=1 eval.
    class CHIFromRNInterface {
    public:
        virtual ~CHIFromRNInterface() noexcept = default;

    public:
        virtual void    TickPreHandshake(uint64_t time) noexcept = 0;
        virtual void    TickPostHandshake(uint64_t time) noexcept { }

    public:
        virtual size_t  GetPortCount() const noexcept = 0;
        virtual std::set<size_t>
                        GetPortIndices() const noexcept = 0;
        virtual size_t  GetPortMaxIndex() const noexcept = 0;

    public:
        // Driven outbound channels (RN -> DUT)
        virtual bool    PushTXREQ(size_t index, const REQFlit& flit) noexcept = 0;
        virtual bool    PushTXRSP(size_t index, const RSPFlit& flit) noexcept = 0;
        virtual bool    PushTXDAT(size_t index, const DATFlit& flit) noexcept = 0;

        // Observed inbound channels (DUT -> RN)
        virtual bool    HasRXRSP(size_t index) const noexcept = 0;
        virtual std::optional<RSPFlit>
                        PeekRXRSP(size_t index) const noexcept = 0;
        virtual std::optional<RSPFlit>
                        PopRXRSP(size_t index) noexcept = 0;

        virtual bool    HasRXDAT(size_t index) const noexcept = 0;
        virtual std::optional<DATFlit>
                        PeekRXDAT(size_t index) const noexcept = 0;
        virtual std::optional<DATFlit>
                        PopRXDAT(size_t index) noexcept = 0;

        virtual bool    HasRXSNP(size_t index) const noexcept = 0;
        virtual std::optional<SNPFlit>
                        PeekRXSNP(size_t index) const noexcept = 0;
        virtual std::optional<SNPFlit>
                        PopRXSNP(size_t index) noexcept = 0;

    public:
        virtual bool    SetAllTXREQBufferSize(size_t bufferSize) noexcept { return true; }
        virtual bool    SetAllTXRSPBufferSize(size_t bufferSize) noexcept { return true; }
        virtual bool    SetAllTXDATBufferSize(size_t bufferSize) noexcept { return true; }
    };


    // CHIToRNInterface: the agent acts as the downstream (HN/SN) towards an
    // RN DUT, driving inbound responses/data/snoops and observing outbound
    // requests/responses/data.
    class CHIToRNInterface {
    public:
        virtual ~CHIToRNInterface() noexcept = default;

    public:
        virtual void    TickPreHandshake(uint64_t time) noexcept = 0;
        virtual void    TickPostHandshake(uint64_t time) noexcept { }

    public:
        virtual size_t  GetPortCount() const noexcept = 0;
        virtual std::set<size_t>
                        GetPortIndices() const noexcept = 0;
        virtual size_t  GetPortMaxIndex() const noexcept = 0;

    public:
        // Driven inbound channels (agent -> RN DUT)
        virtual bool    PushRXRSP(size_t index, const RSPFlit& flit) noexcept = 0;
        virtual bool    PushRXDAT(size_t index, const DATFlit& flit) noexcept = 0;
        virtual bool    PushRXSNP(size_t index, const SNPFlit& flit) noexcept = 0;

        // Observed outbound channels (RN DUT -> agent)
        virtual bool    HasTXREQ(size_t index) const noexcept = 0;
        virtual std::optional<REQFlit>
                        PeekTXREQ(size_t index) const noexcept = 0;
        virtual std::optional<REQFlit>
                        PopTXREQ(size_t index) noexcept = 0;

        virtual bool    HasTXRSP(size_t index) const noexcept = 0;
        virtual std::optional<RSPFlit>
                        PeekTXRSP(size_t index) const noexcept = 0;
        virtual std::optional<RSPFlit>
                        PopTXRSP(size_t index) noexcept = 0;

        virtual bool    HasTXDAT(size_t index) const noexcept = 0;
        virtual std::optional<DATFlit>
                        PeekTXDAT(size_t index) const noexcept = 0;
        virtual std::optional<DATFlit>
                        PopTXDAT(size_t index) noexcept = 0;

    public:
        virtual bool    SetAllRXRSPBufferSize(size_t bufferSize) noexcept { return true; }
        virtual bool    SetAllRXDATBufferSize(size_t bufferSize) noexcept { return true; }
        virtual bool    SetAllRXSNPBufferSize(size_t bufferSize) noexcept { return true; }
    };


    // CHIFromSNInterface: the agent acts as the SN, driving responses/data
    // towards the DUT and observing inbound requests/data.
    class CHIFromSNInterface {
    public:
        virtual ~CHIFromSNInterface() noexcept = default;

    public:
        virtual void    TickPreHandshake(uint64_t time) noexcept = 0;
        virtual void    TickPostHandshake(uint64_t time) noexcept { }

    public:
        virtual size_t  GetPortCount() const noexcept = 0;
        virtual std::set<size_t>
                        GetPortIndices() const noexcept = 0;
        virtual size_t  GetPortMaxIndex() const noexcept = 0;

    public:
        // Driven outbound channels (SN -> DUT)
        virtual bool    PushTXRSP(size_t index, const RSPFlit& flit) noexcept = 0;
        virtual bool    PushTXDAT(size_t index, const DATFlit& flit) noexcept = 0;

        // Observed inbound channels (DUT -> SN)
        virtual bool    HasRXREQ(size_t index) const noexcept = 0;
        virtual std::optional<REQFlit>
                        PeekRXREQ(size_t index) const noexcept = 0;
        virtual std::optional<REQFlit>
                        PopRXREQ(size_t index) noexcept = 0;

        virtual bool    HasRXDAT(size_t index) const noexcept = 0;
        virtual std::optional<DATFlit>
                        PeekRXDAT(size_t index) const noexcept = 0;
        virtual std::optional<DATFlit>
                        PopRXDAT(size_t index) noexcept = 0;

    public:
        virtual bool    SetAllTXRSPBufferSize(size_t bufferSize) noexcept { return true; }
        virtual bool    SetAllTXDATBufferSize(size_t bufferSize) noexcept { return true; }
    };


    // CHIToSNInterface: the agent acts as the upstream (HN) towards an SN
    // DUT, driving inbound requests/data and observing outbound
    // responses/data.
    class CHIToSNInterface {
    public:
        virtual ~CHIToSNInterface() noexcept = default;

    public:
        virtual void    TickPreHandshake(uint64_t time) noexcept = 0;
        virtual void    TickPostHandshake(uint64_t time) noexcept { }

    public:
        virtual size_t  GetPortCount() const noexcept = 0;
        virtual std::set<size_t>
                        GetPortIndices() const noexcept = 0;
        virtual size_t  GetPortMaxIndex() const noexcept = 0;

    public:
        // Driven inbound channels (agent -> SN DUT)
        virtual bool    PushRXREQ(size_t index, const REQFlit& flit) noexcept = 0;
        virtual bool    PushRXDAT(size_t index, const DATFlit& flit) noexcept = 0;

        // Observed outbound channels (SN DUT -> agent)
        virtual bool    HasTXRSP(size_t index) const noexcept = 0;
        virtual std::optional<RSPFlit>
                        PeekTXRSP(size_t index) const noexcept = 0;
        virtual std::optional<RSPFlit>
                        PopTXRSP(size_t index) noexcept = 0;

        virtual bool    HasTXDAT(size_t index) const noexcept = 0;
        virtual std::optional<DATFlit>
                        PeekTXDAT(size_t index) const noexcept = 0;
        virtual std::optional<DATFlit>
                        PopTXDAT(size_t index) noexcept = 0;

    public:
        virtual bool    SetAllRXREQBufferSize(size_t bufferSize) noexcept { return true; }
        virtual bool    SetAllRXDATBufferSize(size_t bufferSize) noexcept { return true; }
    };


    // CHIMonitorInterface: passive observer of all CHI channels on a monitor
    // tap. No pins are driven.
    class CHIMonitorInterface {
    public:
        virtual ~CHIMonitorInterface() noexcept = default;

    public:
        virtual void    TickPostHandshake(uint64_t time) noexcept = 0;

    public:
        virtual size_t  GetPortCount() const noexcept = 0;
        virtual std::set<size_t>
                        GetPortIndices() const noexcept = 0;
        virtual size_t  GetPortMaxIndex() const noexcept = 0;

    public:
        // RN-side monitor taps
        virtual bool    HasTXREQ(size_t index) const noexcept = 0;
        virtual std::optional<FiredREQ>
                        PopTXREQ(size_t index) noexcept = 0;

        virtual bool    HasTXRSP(size_t index) const noexcept = 0;
        virtual std::optional<FiredRSP>
                        PopTXRSP(size_t index) noexcept = 0;

        virtual bool    HasTXDAT(size_t index) const noexcept = 0;
        virtual std::optional<FiredDAT>
                        PopTXDAT(size_t index) noexcept = 0;

        virtual bool    HasRXRSP(size_t index) const noexcept = 0;
        virtual std::optional<FiredRSP>
                        PopRXRSP(size_t index) noexcept = 0;

        virtual bool    HasRXDAT(size_t index) const noexcept = 0;
        virtual std::optional<FiredDAT>
                        PopRXDAT(size_t index) noexcept = 0;

        virtual bool    HasRXSNP(size_t index) const noexcept = 0;
        virtual std::optional<FiredSNP>
                        PopRXSNP(size_t index) noexcept = 0;

        // SN-side monitor taps
        virtual bool    HasSN_RXREQ(size_t index) const noexcept = 0;
        virtual std::optional<FiredREQ>
                        PopSN_RXREQ(size_t index) noexcept = 0;

        virtual bool    HasSN_TXRSP(size_t index) const noexcept = 0;
        virtual std::optional<FiredRSP>
                        PopSN_TXRSP(size_t index) noexcept = 0;

        virtual bool    HasSN_TXDAT(size_t index) const noexcept = 0;
        virtual std::optional<FiredDAT>
                        PopSN_TXDAT(size_t index) noexcept = 0;

        virtual bool    HasSN_RXDAT(size_t index) const noexcept = 0;
        virtual std::optional<FiredDAT>
                        PopSN_RXDAT(size_t index) noexcept = 0;
    };


    // CHIMonitor: passive observer of CHI traffic.
    //
    // The bus-facing API consists only of Push functions on all channels.
    // Each Push fires the matching accept event and feeds the transaction
    // tracker. When constructed with a CHIMonitorInterface, Tick drains the
    // observed beats of all channels into the Push functions.
    class CHIMonitor {
    public:
        class EventHub {
        public:
            Gravity::EventBus<TXREQAcceptEvent>     OnTXREQAccept;
            Gravity::EventBus<TXRSPAcceptEvent>     OnTXRSPAccept;
            Gravity::EventBus<TXDATAcceptEvent>     OnTXDATAccept;
            Gravity::EventBus<RXRSPAcceptEvent>     OnRXRSPAccept;
            Gravity::EventBus<RXDATAcceptEvent>     OnRXDATAccept;
            Gravity::EventBus<RXSNPAcceptEvent>     OnRXSNPAccept;
            Gravity::EventBus<RXREQAcceptEvent>     OnRXREQAccept;

        public:
            EventHub() noexcept = default;
        };

    public:
        std::shared_ptr<EventHub>   events = std::make_shared<EventHub>();

    protected:
        std::shared_ptr<CHIMonitorInterface>    busInterface;
        size_t                                  busPortIndex    = 0;
        uint64_t                                time            = 0;

    public:
        CHIMonitor() noexcept = default;
        CHIMonitor(std::shared_ptr<CHIMonitorInterface> busInterface, size_t portIndex) noexcept;

    public:
        void    TickPostHandshake(uint64_t time) noexcept;

    public:
        void    PushTXREQ(const FiredREQ& fired) noexcept;
        void    PushTXRSP(const FiredRSP& fired) noexcept;
        void    PushTXDAT(const FiredDAT& fired) noexcept;
        void    PushRXRSP(const FiredRSP& fired) noexcept;
        void    PushRXDAT(const FiredDAT& fired) noexcept;
        void    PushRXSNP(const FiredSNP& fired) noexcept;
        void    PushRXREQ(const FiredREQ& fired) noexcept;
    };
}
