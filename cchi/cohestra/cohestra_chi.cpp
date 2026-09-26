#include "cohestra_chi.hpp"


// Implementation of: Cohestra::CHI::EventBase
namespace Cohestra::CHI {

    EventBase::EventBase(uint64_t time) noexcept
        : time  (time)
    { }

    uint64_t EventBase::GetTime() const noexcept
    {
        return time;
    }
}


// Implementation of: per-channel accept events
namespace Cohestra::CHI {

    TXREQAcceptEvent::TXREQAcceptEvent(uint64_t time, const FiredREQ& fired) noexcept
        : EventBase (time)
        , fired     (fired)
    { }

    const FiredREQ& TXREQAcceptEvent::GetFired() const noexcept
    {
        return fired;
    }


    TXRSPAcceptEvent::TXRSPAcceptEvent(uint64_t time, const FiredRSP& fired) noexcept
        : EventBase (time)
        , fired     (fired)
    { }

    const FiredRSP& TXRSPAcceptEvent::GetFired() const noexcept
    {
        return fired;
    }


    TXDATAcceptEvent::TXDATAcceptEvent(uint64_t time, const FiredDAT& fired) noexcept
        : EventBase (time)
        , fired     (fired)
    { }

    const FiredDAT& TXDATAcceptEvent::GetFired() const noexcept
    {
        return fired;
    }


    RXRSPAcceptEvent::RXRSPAcceptEvent(uint64_t time, const FiredRSP& fired) noexcept
        : EventBase (time)
        , fired     (fired)
    { }

    const FiredRSP& RXRSPAcceptEvent::GetFired() const noexcept
    {
        return fired;
    }


    RXDATAcceptEvent::RXDATAcceptEvent(uint64_t time, const FiredDAT& fired) noexcept
        : EventBase (time)
        , fired     (fired)
    { }

    const FiredDAT& RXDATAcceptEvent::GetFired() const noexcept
    {
        return fired;
    }


    RXSNPAcceptEvent::RXSNPAcceptEvent(uint64_t time, const FiredSNP& fired) noexcept
        : EventBase (time)
        , fired     (fired)
    { }

    const FiredSNP& RXSNPAcceptEvent::GetFired() const noexcept
    {
        return fired;
    }


    RXREQAcceptEvent::RXREQAcceptEvent(uint64_t time, const FiredREQ& fired) noexcept
        : EventBase (time)
        , fired     (fired)
    { }

    const FiredREQ& RXREQAcceptEvent::GetFired() const noexcept
    {
        return fired;
    }
}


// Implementation of: Cohestra::CHI::CHIMonitor
namespace Cohestra::CHI {

    CHIMonitor::CHIMonitor(std::shared_ptr<CHIMonitorInterface> busInterface,
                           size_t portIndex) noexcept
        : busInterface  (busInterface)
        , busPortIndex  (portIndex)
    { }

    void CHIMonitor::TickPostHandshake(uint64_t time) noexcept
    {
        this->time = time;

        if (!busInterface)
            return;

        busInterface->TickPostHandshake(time);

        while (auto fired = busInterface->PopTXREQ(busPortIndex))
            PushTXREQ(*fired);

        while (auto fired = busInterface->PopTXRSP(busPortIndex))
            PushTXRSP(*fired);

        while (auto fired = busInterface->PopTXDAT(busPortIndex))
            PushTXDAT(*fired);

        while (auto fired = busInterface->PopRXRSP(busPortIndex))
            PushRXRSP(*fired);

        while (auto fired = busInterface->PopRXDAT(busPortIndex))
            PushRXDAT(*fired);

        while (auto fired = busInterface->PopRXSNP(busPortIndex))
            PushRXSNP(*fired);

        while (auto fired = busInterface->PopSN_RXREQ(busPortIndex))
            PushRXREQ(*fired);

        while (auto fired = busInterface->PopSN_TXRSP(busPortIndex))
            PushTXRSP(*fired);

        while (auto fired = busInterface->PopSN_TXDAT(busPortIndex))
            PushTXDAT(*fired);

        while (auto fired = busInterface->PopSN_RXDAT(busPortIndex))
            PushRXDAT(*fired);
    }

    void CHIMonitor::PushTXREQ(const FiredREQ& fired) noexcept
    {
        events->OnTXREQAccept(fired.Time, fired);
    }

    void CHIMonitor::PushTXRSP(const FiredRSP& fired) noexcept
    {
        events->OnTXRSPAccept(fired.Time, fired);
    }

    void CHIMonitor::PushTXDAT(const FiredDAT& fired) noexcept
    {
        events->OnTXDATAccept(fired.Time, fired);
    }

    void CHIMonitor::PushRXRSP(const FiredRSP& fired) noexcept
    {
        events->OnRXRSPAccept(fired.Time, fired);
    }

    void CHIMonitor::PushRXDAT(const FiredDAT& fired) noexcept
    {
        events->OnRXDATAccept(fired.Time, fired);
    }

    void CHIMonitor::PushRXSNP(const FiredSNP& fired) noexcept
    {
        events->OnRXSNPAccept(fired.Time, fired);
    }

    void CHIMonitor::PushRXREQ(const FiredREQ& fired) noexcept
    {
        events->OnRXREQAccept(fired.Time, fired);
    }
}
