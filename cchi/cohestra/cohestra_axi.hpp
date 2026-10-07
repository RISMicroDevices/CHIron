#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <vector>

#include "cohestra_config.hpp"

#include "spdlog/spdlog.h"

#include "../../common/eventbus.hpp"


namespace Cohestra::AXI {

    // AXI4 burst types (AWBURST / ARBURST)
    enum class Burst : uint8_t {
        FIXED   = 0b00,
        INCR    = 0b01,
        WRAP    = 0b10
    };

    // AXI4 response codes (BRESP / RRESP)
    enum class Resp : uint8_t {
        OKAY    = 0b00,
        EXOKAY  = 0b01,
        SLVERR  = 0b10,
        DECERR  = 0b11
    };

    const char* ToString(Burst burst) noexcept;
    const char* ToString(Resp resp) noexcept;


    // Denial codes of the AXI components
    class DenialEnumBack {
    public:
        const char* name;
        const int   value;
        const bool  isAccepted;
        const bool  isRejected;

    public:
        inline constexpr DenialEnumBack(const char* name, const int value, bool isAccepted = false, bool isRejected = false) noexcept
        : name(name), value(value), isAccepted(isAccepted), isRejected(isRejected) { }

    public:
        inline constexpr bool IsDone() const noexcept
        { return !isAccepted && !isRejected; }

        inline constexpr bool IsAccepted() const noexcept
        { return isAccepted; }

        inline constexpr bool IsRejected() const noexcept
        { return isRejected; }

    public:
        inline constexpr operator int() const noexcept
        { return value; }

        inline constexpr operator const DenialEnumBack*() const noexcept
        { return this; }

        inline constexpr bool operator==(const DenialEnumBack& obj) const noexcept
        { return value == obj.value; }

        inline constexpr bool operator!=(const DenialEnumBack& obj) const noexcept
        { return !(*this == obj); }
    };

    using DenialEnum = const DenialEnumBack*;

    namespace Denial {
        inline constexpr DenialEnumBack NOT_INITIALIZED       ("NOT_INITIALIZED",       0xFFFF0000 |  0, false, false);

        inline constexpr DenialEnumBack DONE                  ("DONE",                  0x00000000 |  0, false, false);
        inline constexpr DenialEnumBack ACCEPTED              ("ACCEPTED",              0x00000000 |  1, true , false);

        inline constexpr DenialEnumBack REJECTED              ("REJECTED",              0x00010000 |  0, false, true );
        inline constexpr DenialEnumBack REJECTED_WRITE_DEPTH  ("REJECTED_WRITE_DEPTH",  0x00010000 |  1, false, true );
        inline constexpr DenialEnumBack REJECTED_READ_DEPTH   ("REJECTED_READ_DEPTH",   0x00010000 |  2, false, true );
        inline constexpr DenialEnumBack REJECTED_ARGUMENT     ("REJECTED_ARGUMENT",     0x00010000 |  3, false, true );
    }


    // FutureNow: placeholder of an accepted-or-rejected access result.
    // An accepted access holds no event yet (IsFuture) and fires its bound
    // callbacks once on completion; a completed access (IsNow) runs newly
    // bound callbacks immediately; a rejected access (IsRejected) never fires.
    template<class TEvent>
    class FutureNow {
    public:
        using func_t = std::function<void(const TEvent&)>;

    protected:
        DenialEnum              denial;
        std::optional<TEvent>   event;
        std::vector<func_t>     future;

        size_t                  firedFutureCount;

    public:
        FutureNow(DenialEnum denial = Denial::NOT_INITIALIZED) noexcept;
        FutureNow(DenialEnum denial, const TEvent& event) noexcept;

    public:
        bool        Bind(func_t func) noexcept;
        bool        BindNow(func_t func) noexcept;
        bool        BindFuture(func_t func) noexcept;

    public:
        bool        IsRejected() const noexcept;
        bool        IsAccepted() const noexcept;
        bool        IsDone() const noexcept;

        bool        IsFuture() const noexcept;
        bool        IsNow() const noexcept;

        DenialEnum  GetDenial() const noexcept;

        size_t      Fired() const noexcept;

    public:
        void        Fire(const TEvent& event) noexcept;
    };


    // Decoupled (valid/ready) channel base
    class DecoupledBundle {
    public:
        uint8_t     Valid   = 0;
        uint8_t     Ready   = 0;

    public:
        bool        Fire() const noexcept { return Valid && Ready; }
    };

    // AXI4 write address channel (AW)
    class BundleChannelAW : public DecoupledBundle {
    public:
        uint32_t    Id      = 0;
        uint64_t    Addr    = 0;
        uint8_t     Len     = 0;
        uint8_t     Size    = 0;
        AXI::Burst  Burst   = AXI::Burst::FIXED;
    };

    // AXI4 write data channel (W)
    // 'Data' holds the beat in 64-bit lanes (little-endian byte order per lane).
    // 'Strb' holds the write strobes bit-packed into 64-bit lanes.
    class BundleChannelW : public DecoupledBundle {
    public:
        std::vector<uint64_t>   Data;
        std::vector<uint64_t>   Strb;
        uint8_t                 Last    = 0;
    };

    // AXI4 write response channel (B)
    class BundleChannelB : public DecoupledBundle {
    public:
        uint32_t    Id      = 0;
        AXI::Resp   Resp    = AXI::Resp::OKAY;
    };

    // AXI4 read address channel (AR)
    class BundleChannelAR : public DecoupledBundle {
    public:
        uint32_t    Id      = 0;
        uint64_t    Addr    = 0;
        uint8_t     Len     = 0;
        uint8_t     Size    = 0;
        AXI::Burst  Burst   = AXI::Burst::FIXED;
    };

    // AXI4 read data channel (R)
    // 'Data' holds the beat in 64-bit lanes (little-endian byte order per lane).
    class BundleChannelR : public DecoupledBundle {
    public:
        uint32_t                Id      = 0;
        std::vector<uint64_t>   Data;
        AXI::Resp               Resp    = AXI::Resp::OKAY;
        uint8_t                 Last    = 0;
    };

    // AXI4 5-channel bundle of a single port
    class Bundle {
    public:
        BundleChannelAW     AW;
        BundleChannelW      W;
        BundleChannelB      B;
        BundleChannelAR     AR;
        BundleChannelR      R;
    };


    // Channel payload captured at a completed handshake
    template<class TChannel>
    class FiredChannel {
    public:
        uint64_t    Time    = 0;
        TChannel    Bundle;
    };

    using FiredChannelAW    = FiredChannel<BundleChannelAW>;
    using FiredChannelW     = FiredChannel<BundleChannelW>;
    using FiredChannelB     = FiredChannel<BundleChannelB>;
    using FiredChannelAR    = FiredChannel<BundleChannelAR>;
    using FiredChannelR     = FiredChannel<BundleChannelR>;


    // Byte 'byte' of a lane-vector payload (little-endian byte order per lane)
    inline uint8_t DataByteAt(const std::vector<uint64_t>& data, size_t byte) noexcept
    {
        size_t lane = byte / 8;
        if (lane >= data.size())
            return 0;

        return uint8_t(data[lane] >> ((byte % 8) * 8));
    }

    // Bit 'bit' of a lane-vector bit field
    inline bool StrbBitAt(const std::vector<uint64_t>& strb, size_t bit) noexcept
    {
        size_t lane = bit / 64;
        if (lane >= strb.size())
            return false;

        return (strb[lane] >> (bit % 64)) & 1;
    }

    // Next beat address of an AXI4 burst
    // *NOTICE: The WRAP boundary is computed from the CURRENT beat address
    //          (which always lies inside the wrap region); tl-test-new's
    //          MemoryAgent computed it from the next address, where the
    //          comparison could never hold and WRAP behaved like INCR.
    inline uint64_t AdvanceBurstAddress(uint64_t address, AXI::Burst burst,
                                        size_t numBytes, size_t burstLen) noexcept
    {
        switch (burst)
        {
            case AXI::Burst::FIXED:
                break;

            case AXI::Burst::INCR:
                address = address + numBytes;
                break;

            case AXI::Burst::WRAP:
            {
                uint64_t wrapBase = address & ~(numBytes * burstLen - 1);
                address = address + numBytes;
                if (address == wrapBase + numBytes * burstLen)
                    address = wrapBase;
                break;
            }

            default:
                break;
        }

        return address;
    }
}


namespace Cohestra {

    // AXISlaveInterface: abstract device-side view of the AXI4 ports of a
    // simulated module, serving AXI4 memory (slave) semantics.
    //
    // AW / W / AR are driven by the simulated module (upstream -> downstream):
    // they are observed with Has/Peek and acknowledged with Pop. B / R are
    // driven towards the simulated module (downstream -> upstream) with Push.
    //
    // Pop on AW / W / AR asserts the matching 'ready' for the current tick
    // (cleared on the next TickPreHandshake), completing the handshake. Pushed
    // B / R payloads are queued and driven onto the module pins on
    // TickPreHandshake.
    //
    // TickPreHandshake (drive) -> Eval (clock=0) -> TickPostHandshake (sample + pop) -> Eval (clock=1)
    //
    // The retire (pop) of driven B / R beats happens in TickPostHandshake on
    // the mid-cycle settled valid/ready — exactly what the module registers at
    // the upcoming posedge. Never retire on the pins read at cycle start: those
    // reflect the post-posedge state and diverge whenever the module's 'ready'
    // rises exactly at a posedge.
    //
    // Both phases are idempotent within the same timestamp: repeated calls at
    // an already-ticked time are no-ops, so multiple AXIMemory instances
    // sharing one interface can each tick it inside their own ticks.
    class AXISlaveInterface {
    public:
        virtual ~AXISlaveInterface() noexcept = default;

    public:
        virtual void    TickPreHandshake(uint64_t time) noexcept = 0;
        virtual void    TickPostHandshake(uint64_t time) noexcept { }

    public:
        virtual size_t  GetPortCount() const noexcept = 0;

        virtual std::set<size_t>
                        GetPortIndices() const noexcept = 0;

        virtual size_t  GetPortMaxIndex() const noexcept = 0;

        // Data width of a port in bits, one of { 64, 128, 256, 512, 1024 },
        // or 0 when the port is absent.
        virtual size_t  GetPortDataBits(size_t index) const noexcept = 0;

    public:
        virtual bool    HasAW(size_t index) const noexcept = 0;
        virtual std::optional<AXI::BundleChannelAW>
                        PeekAW(size_t index) const noexcept = 0;
        virtual std::optional<AXI::BundleChannelAW>
                        PopAW(size_t index) noexcept = 0;

        virtual bool    HasW(size_t index) const noexcept = 0;
        virtual std::optional<AXI::BundleChannelW>
                        PeekW(size_t index) const noexcept = 0;
        virtual std::optional<AXI::BundleChannelW>
                        PopW(size_t index) noexcept = 0;

        virtual bool    HasAR(size_t index) const noexcept = 0;
        virtual std::optional<AXI::BundleChannelAR>
                        PeekAR(size_t index) const noexcept = 0;
        virtual std::optional<AXI::BundleChannelAR>
                        PopAR(size_t index) noexcept = 0;

        virtual bool    PushB(size_t index, const AXI::BundleChannelB& channel) noexcept = 0;

        virtual bool    PushR(size_t index, const AXI::BundleChannelR& channel) noexcept = 0;
    };
}


namespace Cohestra {

    // AXIMonitorInterface: abstract passive view of the AXI4 ports of a
    // simulated module, observing AXI4 bus traffic without disturbing it.
    //
    // All five channels are observed passively: Has reports a beat whose
    // handshake completed on the current tick, and Pop returns the observed
    // time-stamped payload, consuming the observation.
    //
    // TickPostHandshake -> Observe -> Eval (clock=1)
    //
    // TickPostHandshake runs mid-cycle (after the clock=0 eval, before the
    // posedge eval) and is idempotent within the same timestamp: repeated
    // calls at an already-ticked time are no-ops, so multiple AXIMonitor
    // instances sharing one interface can each tick it inside their own ticks.
    class AXIMonitorInterface {
    public:
        virtual ~AXIMonitorInterface() noexcept = default;

    public:
        virtual void    TickPostHandshake(uint64_t time) noexcept = 0;

    public:
        virtual size_t  GetPortCount() const noexcept = 0;

        virtual std::set<size_t>
                        GetPortIndices() const noexcept = 0;

        virtual size_t  GetPortMaxIndex() const noexcept = 0;

        // Data width of a port in bits, one of { 64, 128, 256, 512, 1024 },
        // or 0 when the port is absent.
        virtual size_t  GetPortDataBits(size_t index) const noexcept = 0;

    public:
        virtual bool    HasAW(size_t index) const noexcept = 0;
        virtual std::optional<AXI::FiredChannelAW>
                        PopAW(size_t index) noexcept = 0;

        virtual bool    HasW(size_t index) const noexcept = 0;
        virtual std::optional<AXI::FiredChannelW>
                        PopW(size_t index) noexcept = 0;

        virtual bool    HasB(size_t index) const noexcept = 0;
        virtual std::optional<AXI::FiredChannelB>
                        PopB(size_t index) noexcept = 0;

        virtual bool    HasAR(size_t index) const noexcept = 0;
        virtual std::optional<AXI::FiredChannelAR>
                        PopAR(size_t index) noexcept = 0;

        virtual bool    HasR(size_t index) const noexcept = 0;
        virtual std::optional<AXI::FiredChannelR>
                        PopR(size_t index) noexcept = 0;
    };
}


namespace Cohestra {

    // AXIMasterInterface: abstract master-side view of the AXI4 ports of a
    // simulated module, driving AXI4 master (initiator) semantics.
    //
    // AW / W / AR are driven towards the simulated module (upstream ->
    // downstream) with Push; Push returns false when the module would not
    // assert 'ready' this tick. B / R are driven by the simulated module
    // (downstream -> upstream): they are observed with Has/Peek and
    // acknowledged with Pop.
    //
    // TickPreHandshake (drive) -> Eval (clock=0) -> TickPostHandshake (sample + pop) -> Eval (clock=1)
    //
    // The retire (pop) of driven AW / W / AR beats happens in TickPostHandshake
    // on the mid-cycle settled valid/ready — exactly what the module registers
    // at the upcoming posedge. Never retire on the pins read at cycle start:
    // those reflect the post-posedge state and diverge whenever the module's
    // 'ready' rises exactly at a posedge.
    //
    // Both phases are idempotent within the same timestamp: repeated calls at
    // an already-ticked time are no-ops, so multiple AXIMaster instances
    // sharing one interface can each tick it inside their own ticks.
    class AXIMasterInterface {
    public:
        virtual ~AXIMasterInterface() noexcept = default;

    public:
        virtual void    TickPreHandshake(uint64_t time) noexcept = 0;
        virtual void    TickPostHandshake(uint64_t time) noexcept { }

    public:
        virtual size_t  GetPortCount() const noexcept = 0;

        virtual std::set<size_t>
                        GetPortIndices() const noexcept = 0;

        virtual size_t  GetPortMaxIndex() const noexcept = 0;

        // Data width of a port in bits, one of { 64, 128, 256, 512, 1024 },
        // or 0 when the port is absent.
        virtual size_t  GetPortDataBits(size_t index) const noexcept = 0;

    public:
        virtual bool    PushAW(size_t index, const AXI::BundleChannelAW& channel) noexcept = 0;
        virtual bool    PushW(size_t index, const AXI::BundleChannelW& channel) noexcept = 0;
        virtual bool    PushAR(size_t index, const AXI::BundleChannelAR& channel) noexcept = 0;

        virtual bool    HasB(size_t index) const noexcept = 0;
        virtual std::optional<AXI::BundleChannelB>
                        PeekB(size_t index) const noexcept = 0;
        virtual std::optional<AXI::BundleChannelB>
                        PopB(size_t index) noexcept = 0;

        virtual bool    HasR(size_t index) const noexcept = 0;
        virtual std::optional<AXI::BundleChannelR>
                        PeekR(size_t index) const noexcept = 0;
        virtual std::optional<AXI::BundleChannelR>
                        PopR(size_t index) noexcept = 0;
    };
}


namespace Cohestra {

    // Base of AXI bus events
    class AXIEventBase {
    protected:
        uint64_t    time;

    public:
        AXIEventBase(uint64_t time) noexcept;

    public:
        uint64_t    GetTime() const noexcept;
    };

    // Fired on a completed AW handshake (write accept)
    class AXIWriteAcceptEvent : public AXIEventBase
                              , public Gravity::Event<AXIWriteAcceptEvent> {
    protected:
        AXI::FiredChannelAW channel;

    public:
        AXIWriteAcceptEvent(uint64_t time, const AXI::FiredChannelAW& channel) noexcept;

    public:
        const AXI::FiredChannelAW&  GetChannel() const noexcept;
    };

    // Fired on a completed W handshake (write data beat)
    class AXIWriteDataEvent : public AXIEventBase
                            , public Gravity::Event<AXIWriteDataEvent> {
    protected:
        AXI::FiredChannelW channel;

    public:
        AXIWriteDataEvent(uint64_t time, const AXI::FiredChannelW& channel) noexcept;

    public:
        const AXI::FiredChannelW&   GetChannel() const noexcept;
    };

    // Fired on a completed B handshake (write response)
    class AXIWriteResponseEvent : public AXIEventBase
                                , public Gravity::Event<AXIWriteResponseEvent> {
    protected:
        AXI::FiredChannelB channel;

    public:
        AXIWriteResponseEvent(uint64_t time, const AXI::FiredChannelB& channel) noexcept;

    public:
        const AXI::FiredChannelB&   GetChannel() const noexcept;
    };

    // Fired on a completed AR handshake (read accept)
    class AXIReadAcceptEvent : public AXIEventBase
                             , public Gravity::Event<AXIReadAcceptEvent> {
    protected:
        AXI::FiredChannelAR channel;

    public:
        AXIReadAcceptEvent(uint64_t time, const AXI::FiredChannelAR& channel) noexcept;

    public:
        const AXI::FiredChannelAR&  GetChannel() const noexcept;
    };

    // Fired on every completed R handshake (read data beat)
    class AXIReadDataEvent : public AXIEventBase
                           , public Gravity::Event<AXIReadDataEvent> {
    protected:
        AXI::FiredChannelR channel;

    public:
        AXIReadDataEvent(uint64_t time, const AXI::FiredChannelR& channel) noexcept;

    public:
        const AXI::FiredChannelR&   GetChannel() const noexcept;
    };

    // Fired on a completed R handshake with Last asserted (read response)
    class AXIReadResponseEvent : public AXIEventBase
                               , public Gravity::Event<AXIReadResponseEvent> {
    protected:
        AXI::FiredChannelR channel;

    public:
        AXIReadResponseEvent(uint64_t time, const AXI::FiredChannelR& channel) noexcept;

    public:
        const AXI::FiredChannelR&   GetChannel() const noexcept;
    };
}


namespace Cohestra {

    // AXI4 write transaction tracked by AXIMonitor: the persisted request
    // (with its address), the accumulated write data beats, and, once the
    // transaction completes, the final write response.
    //
    // *NOTICE: These records live at namespace scope (instead of being
    //          nested in AXIMonitor) so the transaction events below can
    //          carry them by value.
    class AXIWriteTransaction {
    public:
        AXI::FiredChannelAW             request;
        std::vector<AXI::FiredChannelW> data;
        AXI::FiredChannelB              response;

        bool                            dataDone    = false;

    public:
        AXIWriteTransaction() noexcept = default;
        AXIWriteTransaction(const AXI::FiredChannelAW& request) noexcept;

    public:
        // Replays the accumulated write data into 'acceptor(addr, byte)':
        // every byte whose write strobe is set is delivered with its address
        // computed beat by beat from the request's addr/size/len/burst (the
        // same addressing convention as AXIMemory's write commit). Returns
        // the number of bytes accepted.
        template<class TAcceptor>
        size_t                      WriteTo(TAcceptor&& acceptor) const;
    };

    // AXI4 read transaction tracked by AXIMonitor: the persisted request
    // (with its address) and the accumulated read data beats.
    class AXIReadTransaction {
    public:
        AXI::FiredChannelAR             request;
        std::vector<AXI::FiredChannelR> data;

    public:
        AXIReadTransaction() noexcept = default;
        AXIReadTransaction(const AXI::FiredChannelAR& request) noexcept;

    public:
        // Replays the accumulated read data into 'acceptor(addr, byte)':
        // 'numBytes' bytes of each beat are delivered with addresses computed
        // beat by beat from the request's addr/size/len/burst (the same
        // addressing convention as AXIMemory's read-beat generation). Returns
        // the number of bytes accepted.
        template<class TAcceptor>
        size_t                      WriteTo(TAcceptor&& acceptor) const;
    };


    // Fired when a tracked write transaction completes: the B handshake of a
    // tracked write marks its completion, so the event carries the complete
    // transaction (persisted request, all data beats and the final response).
    class AXIWriteCompleteEvent : public AXIEventBase
                                , public Gravity::Event<AXIWriteCompleteEvent> {
    protected:
        AXIWriteTransaction transaction;

    public:
        AXIWriteCompleteEvent(uint64_t time, const AXIWriteTransaction& transaction) noexcept;

    public:
        const AXIWriteTransaction&  GetTransaction() const noexcept;
    };

    // Fired when a tracked read transaction completes: the R handshake with
    // Last of a tracked read marks its completion, so the event carries the
    // complete transaction (persisted request and all data beats).
    class AXIReadCompleteEvent : public AXIEventBase
                               , public Gravity::Event<AXIReadCompleteEvent> {
    protected:
        AXIReadTransaction transaction;

    public:
        AXIReadCompleteEvent(uint64_t time, const AXIReadTransaction& transaction) noexcept;

    public:
        const AXIReadTransaction&   GetTransaction() const noexcept;
    };
}


namespace Cohestra {

    // AXIMonitor: passive observer of AXI4 bus traffic.
    //
    // The bus-facing API consists of only the Push functions on all five
    // channels. Each Push fires the matching bus event and feeds the
    // transaction tracker:
    //
    //   PushAW -> OnWriteAccept                      (AW fire)
    //   PushW  -> OnWriteData                        (W fire)
    //   PushB  -> OnWriteResponse                    (B fire)
    //             OnWriteComplete                    (write completed)
    //   PushAR -> OnReadAccept                       (AR fire)
    //   PushR  -> OnReadData                         (R fire)
    //             OnReadResponse                     (R fire with Last)
    //             OnReadComplete                     (read completed)
    //
    // The tracker persists the address of every accepted transaction (from
    // AW / AR) and accumulates the final data beats as they arrive. Write
    // data follows the order of the write addresses (AXI4), so a W beat
    // binds to the oldest write with pending data; a B beat completes the
    // oldest write with a matching ID. Read beats bind to the oldest active
    // read of their ID, completing on Last. Completed transactions are
    // published through OnWriteComplete / OnReadComplete and counted
    // ('GetCompletedWriteCount' / 'GetCompletedReadCount').
    //
    // The monitor is passive-tolerant: beats that cannot be matched to a
    // tracked transaction are skipped without failing.
    //
    // An AXIMonitor constructed with an AXIMonitorInterface owns that
    // interface port: Tick ticks the interface and drains the observed
    // beats of all five channels into the Push functions. A default-
    // constructed monitor is purely push-driven (Tick is a no-op).
    class AXIMonitor {
    public:
        class EventHub {
        public:
            Gravity::EventBus<AXIWriteAcceptEvent>      OnWriteAccept;
            Gravity::EventBus<AXIWriteDataEvent>        OnWriteData;
            Gravity::EventBus<AXIWriteResponseEvent>    OnWriteResponse;
            Gravity::EventBus<AXIReadAcceptEvent>       OnReadAccept;
            Gravity::EventBus<AXIReadDataEvent>         OnReadData;
            Gravity::EventBus<AXIReadResponseEvent>     OnReadResponse;
            Gravity::EventBus<AXIWriteCompleteEvent>    OnWriteComplete;
            Gravity::EventBus<AXIReadCompleteEvent>     OnReadComplete;

        public:
            EventHub() noexcept = default;
        };

    public:
        std::shared_ptr<EventHub>   events  = std::make_shared<EventHub>();

    protected:
        std::shared_ptr<AXIMonitorInterface>    busInterface;
        size_t                                  busPortIndex    = 0;

        uint64_t                                time            = 0;

        std::list<AXIWriteTransaction>                      activeWrites;
        std::map<uint32_t, std::deque<AXIReadTransaction>>  activeReads;

        uint64_t                                countCompletedWrites    = 0;
        uint64_t                                countCompletedReads     = 0;

    public:
        AXIMonitor() noexcept = default;
        AXIMonitor(std::shared_ptr<AXIMonitorInterface> busInterface, size_t portIndex) noexcept;

    public:
        // Mid-cycle observation hook (renamed from Tick): ticks the interface
        // and drains the observed beats of all five channels.
        void                        TickPostHandshake(uint64_t time) noexcept;

    public:
        void                        PushAW(const AXI::FiredChannelAW& channel) noexcept;
        void                        PushW(const AXI::FiredChannelW& channel) noexcept;
        void                        PushB(const AXI::FiredChannelB& channel) noexcept;
        void                        PushAR(const AXI::FiredChannelAR& channel) noexcept;
        void                        PushR(const AXI::FiredChannelR& channel) noexcept;

    public:
        uint64_t                    GetCompletedWriteCount() const noexcept;
        uint64_t                    GetCompletedReadCount() const noexcept;
    };
}


namespace Cohestra {

    // AXIMemory: a simple, pure-C++ AXI4 memory (slave) model, ported from
    // tl-test-new's MemoryAgent.
    //
    // The data width is fixed at construction ('dataBits', one of
    // { 64, 128, 256, 512, 1024 }) and never changes afterwards; the width is
    // carried at runtime instead of as a template parameter, and channel
    // payloads travel in 64-bit lane vectors sized by the width.
    //
    // The device side is fed through the Push/Pop interface:
    //   PushAW / PushW / PushAR   accept a channel payload, returning false
    //                             when the memory would not assert 'ready'
    //                             (backpressure denial or protocol violation);
    //   HasB / PeekB / PopB       emit a write response once the configured
    //                             write latency has elapsed;
    //   HasR / PeekR / PopR       emit read data beats once the configured
    //                             read latency has elapsed.
    //
    // A memory constructed with an AXISlaveInterface owns that interface
    // port: its data width is taken from the port ('GetPortDataBits'), and
    // Tick ticks the interface and completes the channel interactions of
    // the port (accepting AW / W / AR with backpressure, driving B / R)
    // on top of the Push/Pop interface above.
    //
    // Write transactions are handled strictly in order (regardless of ID
    // match), so a W beat always binds to the oldest active write. Read
    // bursts pre-generate all of their data beats at accept time; reads may
    // be reordered before their first beat when 'ooor' is configured.
    // Read requests with an ID overlapping an already active read are
    // denied and reported (tl-test-new crashes on this case).
    //
    // The backing store is allocated lazily on first use, after the memory
    // window has been established by LoadConfiguration.
    //
    // Denials are always reported: flow-control (backpressure) denials are
    // counted and logged at debug level, protocol violations are counted and
    // logged at warning level; both are readable through counters.
    class AXIMemory {
    public:
        static const ConfigurationEntryBack<uint64_t>   CONFIG_AXI_MEMORY_START;
        static const ConfigurationEntryBack<uint64_t>   CONFIG_AXI_MEMORY_END;

        static const ConfigurationEntryBack<uint32_t>   CONFIG_AXI_MEMORY_READ_DEPTH;
        static const ConfigurationEntryBack<uint32_t>   CONFIG_AXI_MEMORY_WRITE_DEPTH;

        static const ConfigurationEntryBack<uint32_t>   CONFIG_AXI_MEMORY_READ_LATENCY;
        static const ConfigurationEntryBack<uint32_t>   CONFIG_AXI_MEMORY_WRITE_LATENCY;
        static const ConfigurationEntryBack<uint32_t>   CONFIG_AXI_MEMORY_CYCLE_UNIT;

        static const ConfigurationEntryBack<bool>       CONFIG_AXI_MEMORY_OOOR;

        static const ConfigurationEntryBack<bool>       CONFIG_AXI_MEMORY_VERBOSE;
        static const ConfigurationEntryBack<bool>       CONFIG_AXI_MEMORY_VERBOSE_DATA;

    public:
        class WriteTransaction {
        public:
            AXI::FiredChannelAW             request;
            std::vector<AXI::FiredChannelW> data;
            AXI::FiredChannelB              response;

            bool                            dataDone    = false;

        public:
            WriteTransaction() noexcept = default;
            WriteTransaction(const AXI::FiredChannelAW& request) noexcept;
        };

        class ReadTransaction {
        public:
            AXI::FiredChannelAR             request;
            std::vector<AXI::FiredChannelR> data;

            size_t                          dataPending = 0;

        public:
            ReadTransaction() noexcept = default;
            ReadTransaction(const AXI::FiredChannelAR& request) noexcept;
        };

    protected:
        std::shared_ptr<spdlog::logger>
                                logger              = spdlog::default_logger();

        std::shared_ptr<AXISlaveInterface>
                                busInterface;
        size_t                  busPortIndex        = 0;

        const size_t            dataBits;
        const size_t            dataBytes;
        const size_t            dataLanes;

        uint8_t*                pmem                = nullptr;
        bool                    pmemExternal        = false;

        const unsigned int      seed;
        std::mt19937_64         rand;

        uint64_t                time                = 0;

        uint64_t                memoryStart;
        uint64_t                memoryEnd;

        uint32_t                readDepth;
        uint32_t                writeDepth;

        uint32_t                readLatency;
        uint32_t                writeLatency;
        uint32_t                cycleUnit;

        bool                    ooor;

        // verbose switches: 'verbose' is the total switch of all categories
        bool                    verbose;
        bool                    verboseData;

        // *NOTICE: There is no pendingWrites mechanism because all AXI writes
        //          were handled in-order regardless of ID match.
        std::list<std::shared_ptr<WriteTransaction>>    activeWrites;
        std::list<std::shared_ptr<WriteTransaction>>    finishedWrites;

        std::vector<std::shared_ptr<ReadTransaction>>   activeReads;
        std::shared_ptr<ReadTransaction>                preparedRead;

        uint64_t                countDeniedBackpressure   = 0;
        uint64_t                countDeniedProtocol       = 0;

    public:
        AXIMemory(size_t dataBits, unsigned int seed) noexcept;
        AXIMemory(size_t dataBits, unsigned int seed, uint8_t* pmem) noexcept;
        AXIMemory(std::shared_ptr<AXISlaveInterface> busInterface, size_t portIndex,
                  unsigned int seed) noexcept;

        // Configuration-in-arguments variants: when LoadConfiguration is
        // never called, the configuration fields come from these arguments
        // instead of the CONFIG_AXI_MEMORY_* defaults.
        AXIMemory(size_t dataBits, unsigned int seed,
                  uint64_t memoryStart, uint64_t memoryEnd,
                  uint32_t readDepth, uint32_t writeDepth,
                  uint32_t readLatency, uint32_t writeLatency, uint32_t cycleUnit,
                  bool ooor, bool verbose, bool verboseData) noexcept;
        AXIMemory(size_t dataBits, unsigned int seed, uint8_t* pmem,
                  uint64_t memoryStart, uint64_t memoryEnd,
                  uint32_t readDepth, uint32_t writeDepth,
                  uint32_t readLatency, uint32_t writeLatency, uint32_t cycleUnit,
                  bool ooor, bool verbose, bool verboseData) noexcept;
        AXIMemory(std::shared_ptr<AXISlaveInterface> busInterface, size_t portIndex,
                  unsigned int seed,
                  uint64_t memoryStart, uint64_t memoryEnd,
                  uint32_t readDepth, uint32_t writeDepth,
                  uint32_t readLatency, uint32_t writeLatency, uint32_t cycleUnit,
                  bool ooor, bool verbose, bool verboseData) noexcept;
        virtual ~AXIMemory() noexcept;

    public:
        bool                                LoadConfiguration(const StringLoaderBase& loader) noexcept;

    public:
        // Cycle-start phase: ticks the interface's drive phase and completes
        // the channel interactions (accepting AW / W / AR, queueing B / R).
        void                                TickPreHandshake(uint64_t time) noexcept;
        // Mid-cycle phase: ticks the interface's retire phase (B / R pops on
        // the settled mid-cycle handshakes).
        void                                TickPostHandshake(uint64_t time) noexcept;

    public:
        bool                                PushAW(const AXI::BundleChannelAW& channel) noexcept;
        bool                                PushW(const AXI::BundleChannelW& channel) noexcept;
        bool                                PushAR(const AXI::BundleChannelAR& channel) noexcept;

        bool                                HasB() const noexcept;
        std::optional<AXI::BundleChannelB>  PeekB() const noexcept;
        std::optional<AXI::BundleChannelB>  PopB() noexcept;

        bool                                HasR() const noexcept;
        std::optional<AXI::BundleChannelR>  PeekR() const noexcept;
        std::optional<AXI::BundleChannelR>  PopR() noexcept;

    public:
        size_t                              GetDataBits() const noexcept;
        size_t                              GetDataBytes() const noexcept;

        uint8_t*                            Memory() noexcept;
        const uint8_t*                      Memory() const noexcept;

        uint8_t&                            Access(uint64_t addr) noexcept;
        uint8_t                             Access(uint64_t addr) const noexcept;

        bool                                Accessible(uint64_t addr) const noexcept;

        size_t                              Size() const noexcept;

        uint64_t                            GetBackpressureDenialCount() const noexcept;
        uint64_t                            GetProtocolDenialCount() const noexcept;

        std::shared_ptr<spdlog::logger>     GetLogger() const noexcept;
        void                                SetLogger(std::shared_ptr<spdlog::logger> logger) noexcept;

    protected:
        bool                                IsWActive(uint32_t wid) const noexcept;

        void                                EnsureAllocated() noexcept;

        void                                ReportBackpressure(const char* what) noexcept;
        void                                ReportViolation(const char* what) noexcept;

        template<class T>
        bool                                LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept;
    };
}


namespace Cohestra {

    // AXIMaster: a simple, pure-C++ AXI4 master (initiator) model.
    //
    // The data width is fixed at construction ('dataBits', one of
    // { 64, 128, 256, 512, 1024 }, or taken from the port when constructed
    // with an AXIMasterInterface) and never changes afterwards; channel
    // payloads travel in 64-bit lane vectors sized by the width, the same
    // convention as AXIMemory.
    //
    // Accesses are issued with DoWrite / DoRead (or the width helpers
    // Write8..Write1024 / Read8..Read1024) and tracked by the returned
    // FutureNow: accepted requests get an ACCEPTED future fired on completion
    // (B for writes, R with Last for reads) with the finished transaction
    // record; requests that cannot be taken get a rejected future
    // (AXI::Denial::REJECTED_*).
    //
    // The device side is fed through the Push/Pop interface:
    //   HasAW / PeekAW / PopAW, HasW / PeekW / PopW, HasAR / PeekAR / PopAR
    //                         emit the generated request beats;
    //   PushB / PushR           accept the response beats, returning false on
    //                           a protocol violation (no matching active
    //                           transaction).
    //
    // A master constructed with an AXIMasterInterface owns that interface
    // port: Tick ticks the interface and completes the channel interactions
    // of the port (driving AW / W / AR with retry on backpressure denial,
    // accepting B / R) on top of the Push/Pop interface above.
    //
    // Write data beats are driven strictly in write-address order (AXI4), and
    // a W beat is never driven before its AW. IDs are allocated round-robin
    // at issue time, skipping the IDs of all in-flight transactions, so an ID
    // in flight is never reused.
    class AXIMaster {
    public:
        static const ConfigurationEntryBack<uint32_t>   CONFIG_AXI_MASTER_READ_DEPTH;
        static const ConfigurationEntryBack<uint32_t>   CONFIG_AXI_MASTER_WRITE_DEPTH;

        static const ConfigurationEntryBack<bool>       CONFIG_AXI_MASTER_VERBOSE;
        static const ConfigurationEntryBack<bool>       CONFIG_AXI_MASTER_VERBOSE_DATA;

    public:
        // Burst arrangement applied by the width helpers:
        //   FIXED_PREFERRED - FIXED whenever the access fits in a single data
        //                     beat, otherwise an aligned INCR;
        //   ALIGNED_INCR    - INCR starting at the access-size-aligned base;
        //   WRAP_CWF        - WRAP with the beat holding 'addr' driven first
        //                     (critical-word-first).
        enum class AccessMode : uint8_t {
            FIXED_PREFERRED = 0,
            ALIGNED_INCR    = 1,
            WRAP_CWF        = 2
        };

        // Completion payloads of the Do*/Write*/Read* futures: the tracked
        // transaction records shared with AXIMonitor.
        using WriteFuture = std::shared_ptr<AXI::FutureNow<AXIWriteTransaction>>;
        using ReadFuture  = std::shared_ptr<AXI::FutureNow<AXIReadTransaction>>;

    public:
        class WriteTransaction {
        public:
            AXI::FiredChannelAW                 request;
            std::vector<AXI::BundleChannelW>    dataPending;    // beats left to drive
            std::vector<AXI::FiredChannelW>     data;           // beats driven
            AXI::FiredChannelB                  response;

            bool                                requestSent = false;

            WriteFuture                         future;

        public:
            WriteTransaction() noexcept = default;
            WriteTransaction(const AXI::FiredChannelAW& request, WriteFuture future) noexcept;
        };

        class ReadTransaction {
        public:
            AXI::FiredChannelAR                 request;
            std::vector<AXI::FiredChannelR>     data;

            bool                                requestSent = false;

            ReadFuture                          future;

        public:
            ReadTransaction() noexcept = default;
            ReadTransaction(const AXI::FiredChannelAR& request, ReadFuture future) noexcept;
        };

    protected:
        std::shared_ptr<spdlog::logger>
                                logger              = spdlog::default_logger();

        std::shared_ptr<AXIMasterInterface>
                                busInterface;
        size_t                  busPortIndex        = 0;

        const size_t            dataBits;
        const size_t            dataBytes;
        const size_t            dataLanes;

        const unsigned int      seed;
        std::mt19937_64         rand;

        uint64_t                time                = 0;

        uint32_t                readDepth;
        uint32_t                writeDepth;

        // verbose switches: 'verbose' is the total switch of all categories
        bool                    verbose;
        bool                    verboseData;

        // write data follows write address order (AXI4): the front entry
        // streams its W beats once its AW was driven; B completes by ID
        std::deque<std::shared_ptr<WriteTransaction>>   activeWrites;
        std::deque<std::shared_ptr<ReadTransaction>>    activeReads;

        uint32_t                nextWriteId         = 0;
        uint32_t                nextReadId          = 0;

        uint64_t                countDenied          = 0;
        uint64_t                countCompletedWrites = 0;
        uint64_t                countCompletedReads  = 0;

    public:
        AXIMaster(size_t dataBits, unsigned int seed) noexcept;
        AXIMaster(std::shared_ptr<AXIMasterInterface> busInterface, size_t portIndex,
                  unsigned int seed) noexcept;

        // Configuration-in-arguments variants: when LoadConfiguration is
        // never called, the configuration fields come from these arguments
        // instead of the CONFIG_AXI_MASTER_* defaults.
        AXIMaster(size_t dataBits, unsigned int seed,
                  uint32_t readDepth, uint32_t writeDepth,
                  bool verbose, bool verboseData) noexcept;
        AXIMaster(std::shared_ptr<AXIMasterInterface> busInterface, size_t portIndex,
                  unsigned int seed,
                  uint32_t readDepth, uint32_t writeDepth,
                  bool verbose, bool verboseData) noexcept;
        virtual ~AXIMaster() noexcept = default;

    public:
        bool                                LoadConfiguration(const StringLoaderBase& loader) noexcept;

    public:
        // Cycle-start phase: ticks the interface's drive phase and completes
        // the channel interactions (issuing AW / W / AR, acknowledging B / R).
        void                                TickPreHandshake(uint64_t time) noexcept;
        // Mid-cycle phase: ticks the interface's retire phase (AW / W / AR
        // pops on the settled mid-cycle handshakes).
        void                                TickPostHandshake(uint64_t time) noexcept;

    public:
        // Primary access API. 'burst' selects the accessing mode (FIXED /
        // INCR / WRAP), 'numBytes' is the beat size in bytes (a power of two,
        // at most the port data width) and 'burstLen' the beat count (FIXED:
        // at most 16, INCR: at most 256, WRAP: exactly 2, 4, 8 or 16 with a
        // beat-size-aligned address). The write payload travels in 64-bit
        // lanes covering the burst's address span from its base (FIXED:
        // 'numBytes' bytes replayed on every beat; INCR / WRAP:
        // 'numBytes' * 'burstLen' bytes covering the aligned burst window);
        // within a beat the payload lands at the addressed byte lanes with
        // the matching strobes, and no beat may cross the data-bus boundary.
        // Invalid combinations are rejected with
        // AXI::Denial::REJECTED_ARGUMENT; a full outstanding queue is
        // rejected with AXI::Denial::REJECTED_WRITE_DEPTH /
        // AXI::Denial::REJECTED_READ_DEPTH.
        //
        // *NOTICE: read data follows the AXIMemory convention: the meaningful
        //          bytes of each returned beat are its leading 'numBytes'
        //          lanes, covering the data-bus-aligned window of the beat
        //          address (the same convention as AXIReadTransaction's
        //          WriteTo replay).
        WriteFuture                         DoWrite(uint64_t addr, AXI::Burst burst, size_t numBytes,
                                                    size_t burstLen, const std::vector<uint64_t>& data) noexcept;
        ReadFuture                          DoRead(uint64_t addr, AXI::Burst burst, size_t numBytes,
                                                   size_t burstLen) noexcept;

    public:
        // Width helpers. An access that fits in a single data beat is issued
        // as one FIXED beat ('FIXED is preferred'); a wider access is issued
        // as an aligned INCR or a critical-word-first WRAP per 'mode'.
        WriteFuture                         Write8(uint64_t addr, uint8_t value,
                                                   AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;
        WriteFuture                         Write16(uint64_t addr, uint16_t value,
                                                    AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;
        WriteFuture                         Write32(uint64_t addr, uint32_t value,
                                                    AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;
        WriteFuture                         Write64(uint64_t addr, uint64_t value,
                                                    AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;
        WriteFuture                         Write128(uint64_t addr, const uint64_t value[2],
                                                     AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;
        WriteFuture                         Write256(uint64_t addr, const uint64_t value[4],
                                                     AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;
        WriteFuture                         Write512(uint64_t addr, const uint64_t value[8],
                                                     AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;
        WriteFuture                         Write1024(uint64_t addr, const uint64_t value[16],
                                                      AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;

        ReadFuture                          Read8(uint64_t addr,
                                                  AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;
        ReadFuture                          Read16(uint64_t addr,
                                                   AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;
        ReadFuture                          Read32(uint64_t addr,
                                                   AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;
        ReadFuture                          Read64(uint64_t addr,
                                                   AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;
        ReadFuture                          Read128(uint64_t addr,
                                                    AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;
        ReadFuture                          Read256(uint64_t addr,
                                                    AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;
        ReadFuture                          Read512(uint64_t addr,
                                                    AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;
        ReadFuture                          Read1024(uint64_t addr,
                                                     AccessMode mode = AccessMode::FIXED_PREFERRED) noexcept;

    public:
        bool                                HasAW() const noexcept;
        std::optional<AXI::BundleChannelAW> PeekAW() const noexcept;
        std::optional<AXI::BundleChannelAW> PopAW() noexcept;

        bool                                HasW() const noexcept;
        std::optional<AXI::BundleChannelW>  PeekW() const noexcept;
        std::optional<AXI::BundleChannelW>  PopW() noexcept;

        bool                                HasAR() const noexcept;
        std::optional<AXI::BundleChannelAR> PeekAR() const noexcept;
        std::optional<AXI::BundleChannelAR> PopAR() noexcept;

        bool                                PushB(const AXI::BundleChannelB& channel) noexcept;
        bool                                PushR(const AXI::BundleChannelR& channel) noexcept;

    public:
        size_t                              GetDataBits() const noexcept;
        size_t                              GetDataBytes() const noexcept;

        uint64_t                            GetDeniedCount() const noexcept;
        uint64_t                            GetCompletedWriteCount() const noexcept;
        uint64_t                            GetCompletedReadCount() const noexcept;

        std::shared_ptr<spdlog::logger>     GetLogger() const noexcept;
        void                                SetLogger(std::shared_ptr<spdlog::logger> logger) noexcept;

    protected:
        bool                                IsWActive(uint32_t wid) const noexcept;
        bool                                IsRActive(uint32_t rid) const noexcept;

        uint32_t                            AllocateWriteId() noexcept;
        uint32_t                            AllocateReadId() noexcept;

        template<class T>
        bool                                LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept;
    };
}


// Inline implementation of: class AXIWriteTransaction
namespace Cohestra {

    template<class TAcceptor>
    inline size_t AXIWriteTransaction::WriteTo(TAcceptor&& acceptor) const
    {
        size_t      numBytes = size_t(1) << request.Bundle.Size;
        size_t      burstLen = size_t(request.Bundle.Len) + 1;
        uint64_t    address  = request.Bundle.Addr;

        if (request.Bundle.Burst == AXI::Burst::INCR || request.Bundle.Burst == AXI::Burst::WRAP)
            address = address & ~(uint64_t(numBytes) - 1);

        size_t accepted = 0;

        for (const auto& beat : data)
        {
            size_t      beatBytes   = beat.Bundle.Data.size() * 8;
            uint64_t    beatAddress = beatBytes ? (address & ~(uint64_t(beatBytes) - 1)) : address;

            for (size_t i = 0; i < beatBytes; i++)
            {
                if (!AXI::StrbBitAt(beat.Bundle.Strb, i))
                    continue;

                acceptor(beatAddress + i, AXI::DataByteAt(beat.Bundle.Data, i));
                accepted++;
            }

            address = AXI::AdvanceBurstAddress(address, request.Bundle.Burst, numBytes, burstLen);
        }

        return accepted;
    }
}


// Inline implementation of: class AXIReadTransaction
namespace Cohestra {

    template<class TAcceptor>
    inline size_t AXIReadTransaction::WriteTo(TAcceptor&& acceptor) const
    {
        size_t      numBytes = size_t(1) << request.Bundle.Size;
        size_t      burstLen = size_t(request.Bundle.Len) + 1;
        uint64_t    address  = request.Bundle.Addr;

        if (request.Bundle.Burst == AXI::Burst::INCR || request.Bundle.Burst == AXI::Burst::WRAP)
            address = address & ~(uint64_t(numBytes) - 1);

        size_t accepted = 0;

        for (const auto& beat : data)
        {
            size_t      beatBytes   = beat.Bundle.Data.size() * 8;
            uint64_t    beatAddress = beatBytes ? (address & ~(uint64_t(beatBytes) - 1)) : address;

            for (size_t i = 0; i < numBytes && i < beatBytes; i++)
            {
                acceptor(beatAddress + i, AXI::DataByteAt(beat.Bundle.Data, i));
                accepted++;
            }

            address = AXI::AdvanceBurstAddress(address, request.Bundle.Burst, numBytes, burstLen);
        }

        return accepted;
    }
}


// Inline implementation of: class AXIMemory
namespace Cohestra {

    template<class T>
    inline bool AXIMemory::LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept
    {
        std::optional<std::string> rawValue = loader.GetRawString(entry, "memory");

        if (!rawValue)
        {
            dst = entry->defaultValue;
            return true;
        }

        std::optional<T> deserializedValue = entry->deserializer(*rawValue);

        if (!deserializedValue)
        {
            logger->error("Failed to parse {} ({}) from configuration value: {}",
                entry->key, entry->name, *rawValue);

            return false;
        }

        dst = *deserializedValue;
        return true;
    }
}


// Inline implementation of: template<class TEvent> class FutureNow
namespace Cohestra::AXI {

    template<class TEvent>
    inline FutureNow<TEvent>::FutureNow(DenialEnum denial) noexcept
        : denial            (denial)
        , event             ()
        , future            ()
        , firedFutureCount  (0)
    { }

    template<class TEvent>
    inline FutureNow<TEvent>::FutureNow(DenialEnum denial, const TEvent& event) noexcept
        : denial            (denial)
        , event             (event)
        , future            ()
        , firedFutureCount  (0)
    { }

    template<class TEvent>
    inline bool FutureNow<TEvent>::Bind(func_t func) noexcept
    {
        if (IsNow())
        {
            func(*event);
            return true;
        }

        if (IsFuture())
        {
            future.push_back(func);
            return false;
        }

        return true;
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::BindNow(func_t func) noexcept
    {
        if (IsNow())
        {
            func(*event);
            return true;
        }

        return false;
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::BindFuture(func_t func) noexcept
    {
        if (IsFuture())
        {
            future.push_back(func);
            return true;
        }

        return false;
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::IsRejected() const noexcept
    {
        return denial->IsRejected();
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::IsAccepted() const noexcept
    {
        return denial->IsAccepted();
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::IsDone() const noexcept
    {
        return denial->IsDone();
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::IsFuture() const noexcept
    {
        return !event.has_value() && !denial->IsRejected();
    }

    template<class TEvent>
    inline bool FutureNow<TEvent>::IsNow() const noexcept
    {
        return event.has_value() && !denial->IsRejected();
    }

    template<class TEvent>
    inline size_t FutureNow<TEvent>::Fired() const noexcept
    {
        return firedFutureCount;
    }

    template<class TEvent>
    inline DenialEnum FutureNow<TEvent>::GetDenial() const noexcept
    {
        return denial;
    }

    template<class TEvent>
    inline void FutureNow<TEvent>::Fire(const TEvent& event) noexcept
    {
        this->event = event;

        for (auto& func : future)
            func(event);

        future.clear();

        firedFutureCount++;
    }
}


// Inline implementation of: class AXIMaster
namespace Cohestra {

    template<class T>
    inline bool AXIMaster::LoadConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry, T& dst) noexcept
    {
        std::optional<std::string> rawValue = loader.GetRawString(entry, "master");

        if (!rawValue)
        {
            dst = entry->defaultValue;
            return true;
        }

        std::optional<T> deserializedValue = entry->deserializer(*rawValue);

        if (!deserializedValue)
        {
            logger->error("Failed to parse {} ({}) from configuration value: {}",
                entry->key, entry->name, *rawValue);

            return false;
        }

        dst = *deserializedValue;
        return true;
    }
}
