//*****************************************************************************
// hades_interface.hpp — the Cohestra boundary over a HADES-Fabric link.
//
//   HadesCCHIInterface     — Cohestra::CCHIInterface (the Instance boundary)
//   HadesAXISlaveInterface — Cohestra::AXISlaveInterface (the AXIMemory port)
//   HadesCHIMonitorNull    — zero-port CHIMonitorInterface (the 6-channel
//                            PACKING.md map carries no CHI monitor taps;
//                            keeps the CLog.B writer's Open/Drain flow alive)
//
// versus the v3 pin shadow (cohestra_v3/interface.hpp): instead of driving
// DUT pins at TickPreHandshake and sampling them at TickPostHandshake, each
// target cycle's pin event is packed into one h2c token per channel
// (PACKING.md layout) and retired on the FPGA-sampled accept echo that rides
// the c2h tokens of a LATER cycle — the stale-ready hazard of
// interface.hpp:784-797 generalized over a link. Concretely:
//
//   * drive channels (rxevt/rxreq/rxrsp/rxdat + axi b/r): Push* lands in a
//     ResizeableFIFO (via MirrorFIFO, which adds indexed peeking); staging a
//     cycle copies the next unstaged flit into the token (valid=1) and marks
//     it in flight; the flit POPS only when the accept echo for it returns
//     (ch4 tail, FPGA-sampled valid&&ready), never earlier;
//   * receive channels (txsnp/txrsp/txdat + axi aw/w/ar): ready is credit-
//     pre-asserted in the staged token whenever the local queue provably has
//     room (queue + cycles-in-flight < rx_queue); received flits queue
//     locally; Has/Peek/Pop serve the queue, one flit per tick, in order —
//     the Instance only requires Peek/Pop consistency within a tick.
//
// The wrapper contract this layer assumes (published for
// rtl/hades_oceanus_wrapper.sv, see README.md): a token's valid bit ENQUEUES
// a new flit into the channel's in-order skid (depth >= [hades] skid_depth);
// the wrapper holds each skid head on the DUT pins until accepted; the echo
// bit is the FPGA-sampled valid&&ready of that cycle.
//*****************************************************************************
#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <set>

#include "../cohestra_interface.hpp"
#include "../cohestra_axi.hpp"
#include "../cohestra_chi.hpp"
#include "../cohestra_v3/interface.hpp"     // details::ResizeableFIFO (sibling reuse,
                                            // same as cohestra_vcs/vcs_top.hpp)

#include "hades_link.hpp"
#include "hades_packing.hpp"


namespace Cohestra::Hades {

    // MirrorFIFO: details::ResizeableFIFO + indexed peek (the k-th unstaged
    // flit is packed into a token while the head stays unaccepted). Pops stay
    // strictly at the front, on the accept echo.
    template<class T>
    class MirrorFIFO : public details::ResizeableFIFO<T> {
    public:
        const T* PeekAt(size_t index) const noexcept
        {
            if (index >= this->count)
                return nullptr;

            return &this->storage[(this->rptr + index) % this->size];
        }

    public:
        // debug/probe accessors (the store probes validate the mirror math)
        size_t      DebugRptr() const noexcept { return this->rptr; }
        size_t      DebugWptr() const noexcept { return this->wptr; }
        size_t      DebugSize() const noexcept { return this->size; }
        const T&    DebugStorage(size_t index) const noexcept { return this->storage[index]; }
    };

    // [hades] INI boundary knobs (typed entries live in hades_harness.cpp)
    struct BoundaryConfig {
        uint32_t    skidDepth   = 32;   // assumed per-drive-channel wrapper skid depth
                                        // (wrapper: 2*WINDOW_K per hades_oceanus_h2c_skid)
        uint32_t    rxQueue     = 1024; // local c2h flit queue bound per channel
        uint32_t    bubbleCycles = 1024;// post-reset bubble window ([root] reset_preamble):
                                        // drive only idle tokens while the wrapper's
                                        // reset stretch (RST_CYCLES=128) runs — flits
                                        // staged during the stretch are flushed by the
                                        // wrapper without echo, which would leak them
    };

    // HadesFlitStore: the flit FIFOs/queues + the PACKING.md pack/unpack,
    // driven by HadesLink through HadesPinSink. Single Type-1 port (index 0)
    // and single AXI slave port (index 0) — the demo boundary is t1p0 + axi_m0.
    class HadesFlitStore : public HadesPinSink {
    public:
        using FlitEVT   = CCHI::Flits::EVT  <CommonFlitConfigurationType1>;
        using FlitREQ   = CCHI::Flits::REQ  <CommonFlitConfigurationType1>;
        using FlitUpRSP = CCHI::Flits::UpRSP<CommonFlitConfigurationType1>;
        using FlitUpDAT = CCHI::Flits::UpDAT<CommonFlitConfigurationType1>;
        using FlitSNP   = CCHI::Flits::SNP  <CommonFlitConfigurationType1>;
        using FlitDnRSP = CCHI::Flits::DnRSP<CommonFlitConfigurationType1>;
        using FlitDnDAT = CCHI::Flits::DnDAT<CommonFlitConfigurationType1>;

    public:
        explicit HadesFlitStore(BoundaryConfig config = {}) noexcept;

    public:
        void        Clear() noexcept;   // reset: drop every FIFO/queue/inflight

        // post-reset bubble window ([root] reset_preamble), applied from the
        // next staged cycle after a reset; must be >= the wrapper's
        // RST_CYCLES (128) so no staged flit is flushed by the stretcher
        void        SetBubbleCycles(uint32_t cycles) noexcept { config.bubbleCycles = cycles; }

    public:
        // HadesPinSink (driven by HadesLink)
        virtual void        PackCh1(uint64_t cycle, TokenWriter token) noexcept;
        virtual void        PackCh2(uint64_t cycle, TokenWriter token) noexcept;
        virtual void        PackCh3(uint64_t cycle, TokenWriter token) noexcept;

        virtual void        UnpackCh4(uint64_t cycle, TokenReader token) noexcept;
        virtual void        UnpackCh5(uint64_t cycle, TokenReader token) noexcept;
        virtual void        UnpackCh6(uint64_t cycle, TokenReader token) noexcept;

    public:
        // drive-channel views (CCHI interface facade)
        MirrorFIFO<FlitEVT>&        FifoEVT() noexcept    { return fifoEVT; }
        MirrorFIFO<FlitREQ>&        FifoREQ() noexcept    { return fifoREQ; }
        MirrorFIFO<FlitUpRSP>&      FifoUpRSP() noexcept  { return fifoUpRSP; }
        MirrorFIFO<FlitUpDAT>&      FifoUpDAT() noexcept  { return fifoUpDAT; }

        // receive-channel queues
        std::deque<FlitSNP>&        QueueSNP() noexcept   { return queueSNP; }
        std::deque<FlitDnRSP>&      QueueDnRSP() noexcept { return queueDnRSP; }
        std::deque<FlitDnDAT>&      QueueDnDAT() noexcept { return queueDnDAT; }

        // AXI slave shadow views
        MirrorFIFO<AXI::BundleChannelB>&    FifoB() noexcept { return fifoB; }
        MirrorFIFO<AXI::BundleChannelR>&    FifoR() noexcept { return fifoR; }

        std::deque<AXI::BundleChannelAW>&   QueueAW() noexcept { return queueAW; }
        std::deque<AXI::BundleChannelW>&    QueueW()  noexcept { return queueW; }
        std::deque<AXI::BundleChannelAR>&   QueueAR() noexcept { return queueAR; }

    public:
        // protocol-fault counters (logged by the harness at end of run)
        uint64_t    GetFenceBlockedCount() const noexcept  { return fenceBlocked; }
        uint64_t    GetFieldTruncationCount() const noexcept { return fieldTruncations; }

        // debug/probe accessor (mirror-math validation)
        size_t      DebugInflightREQ() const noexcept { return inflightREQ; }

    protected:
        // retire one drive flit on its accept echo (implementation in the .cpp)
        template<class T>
        void ApplyEcho(MirrorFIFO<T>& fifo, std::deque<uint64_t>& stagedCycles,
                       size_t& inflight, bool readyEcho, uint64_t tokenCycle,
                       const char* what) noexcept;

    private:
        BoundaryConfig config;

        // drive FIFOs (pop on accept echo only)
        MirrorFIFO<FlitEVT>             fifoEVT;
        MirrorFIFO<FlitREQ>             fifoREQ;
        MirrorFIFO<FlitUpRSP>           fifoUpRSP;
        MirrorFIFO<FlitUpDAT>           fifoUpDAT;
        MirrorFIFO<AXI::BundleChannelB> fifoB;
        MirrorFIFO<AXI::BundleChannelR> fifoR;

        // staged-but-unaccepted counts (mirror of the wrapper skid contents)
        size_t inflightEVT = 0, inflightREQ = 0, inflightUpRSP = 0;
        size_t inflightUpDAT = 0, inflightB = 0, inflightR = 0;

        // per-drive-channel absolute cycle indices of the staged-but-unaccepted
        // flits, ascending (the drivability mirror for the ready echo; the
        // front is the wrapper's hold-or-skid-head flit)
        std::deque<uint64_t>    stagedEVT;
        std::deque<uint64_t>    stagedREQ;
        std::deque<uint64_t>    stagedUpRSP;
        std::deque<uint64_t>    stagedUpDAT;
        std::deque<uint64_t>    stagedB;
        std::deque<uint64_t>    stagedR;

        // receive queues (c2h; the wrapper pre-asserts its own capture-ready,
        // so these never exert backpressure on the DUT)
        std::deque<FlitSNP>             queueSNP;
        std::deque<FlitDnRSP>           queueDnRSP;
        std::deque<FlitDnDAT>           queueDnDAT;
        std::deque<AXI::BundleChannelAW> queueAW;
        std::deque<AXI::BundleChannelW>  queueW;
        std::deque<AXI::BundleChannelAR> queueAR;

        uint64_t    fenceBlocked       = 0;   // ready echo with no drivable staged flit
                                            // (benign: the wrapper samples the raw
                                            // ready pin every tick, idle cycles included)
        uint64_t    fieldTruncations = 0;   // c2h field wider than the C++ flit config
    };


    // HadesCCHIInterface: Cohestra::CCHIInterface over the link.
    class HadesCCHIInterface : public CCHIInterface {
    public:
        HadesCCHIInterface(std::shared_ptr<HadesLink> link,
                           std::shared_ptr<HadesFlitStore> store) noexcept;
        virtual ~HadesCCHIInterface() noexcept = default;

    public:
        virtual void    TickPreHandshake(uint64_t time) noexcept override;
        virtual void    TickPostHandshake(uint64_t time) noexcept override;

    public:
        virtual size_t  GetType1Count() const noexcept override;

        virtual std::set<size_t>
                        GetType1Indices() const noexcept override;

        virtual size_t  GetType1MaxIndex() const noexcept override;

        virtual bool    HasType1SNP(size_t index) const noexcept override;
        virtual std::optional<CCHI::Flits::SNP<CommonFlitConfigurationType1>>
                        PeekType1SNP(size_t index) const noexcept override;
        virtual std::optional<CCHI::Flits::SNP<CommonFlitConfigurationType1>>
                        PopType1SNP(size_t index) noexcept override;

        virtual bool    HasType1DnRSP(size_t index) const noexcept override;
        virtual std::optional<CCHI::Flits::DnRSP<CommonFlitConfigurationType1>>
                        PeekType1DnRSP(size_t index) const noexcept override;
        virtual std::optional<CCHI::Flits::DnRSP<CommonFlitConfigurationType1>>
                        PopType1DnRSP(size_t index) noexcept override;

        virtual bool    HasType1DnDAT(size_t index) const noexcept override;
        virtual std::optional<CCHI::Flits::DnDAT<CommonFlitConfigurationType1>>
                        PeekType1DnDAT(size_t index) const noexcept override;
        virtual std::optional<CCHI::Flits::DnDAT<CommonFlitConfigurationType1>>
                        PopType1DnDAT(size_t index) noexcept override;

        virtual bool    PushType1EVT(size_t index, const CCHI::Flits::EVT<CommonFlitConfigurationType1>& flit) noexcept override;

        virtual bool    PushType1REQ(size_t index, const CCHI::Flits::REQ<CommonFlitConfigurationType1>& flit) noexcept override;

        virtual bool    PushType1UpRSP(size_t index, const CCHI::Flits::UpRSP<CommonFlitConfigurationType1>& flit) noexcept override;

        virtual bool    PushType1UpDAT(size_t index, const CCHI::Flits::UpDAT<CommonFlitConfigurationType1>& flit) noexcept override;

    public:
        // TX-channel buffer depth control (Instance pre-init). Depth 0 has no
        // v3-style unbuffered direct-drive path over a link: pushes fail.
        bool            SetType1EVTBufferSize(size_t index, size_t bufferSize = 0) noexcept;
        bool            SetType1REQBufferSize(size_t index, size_t bufferSize = 0) noexcept;
        bool            SetType1UpRSPBufferSize(size_t index, size_t bufferSize = 0) noexcept;
        bool            SetType1UpDATBufferSize(size_t index, size_t bufferSize = 0) noexcept;

        bool            SetAllType1EVTBufferSize(size_t bufferSize = 0) noexcept override;
        bool            SetAllType1REQBufferSize(size_t bufferSize = 0) noexcept override;
        bool            SetAllType1UpRSPBufferSize(size_t bufferSize = 0) noexcept override;
        bool            SetAllType1UpDATBufferSize(size_t bufferSize = 0) noexcept override;

    private:
        std::shared_ptr<HadesLink>          link;
        std::shared_ptr<HadesFlitStore>     store;
    };


    // HadesAXISlaveInterface: Cohestra::AXISlaveInterface over the link
    // (port 0 = axi_m0 of the PACKING.md map, 256-bit data).
    class HadesAXISlaveInterface : public AXISlaveInterface {
    public:
        HadesAXISlaveInterface(std::shared_ptr<HadesLink> link,
                               std::shared_ptr<HadesFlitStore> store) noexcept;
        virtual ~HadesAXISlaveInterface() noexcept = default;

    public:
        virtual void    TickPreHandshake(uint64_t time) noexcept override;
        virtual void    TickPostHandshake(uint64_t time) noexcept override;

    public:
        virtual size_t  GetPortCount() const noexcept override;

        virtual std::set<size_t>
                        GetPortIndices() const noexcept override;

        virtual size_t  GetPortMaxIndex() const noexcept override;

        virtual size_t  GetPortDataBits(size_t index) const noexcept override;

    public:
        virtual bool    HasAW(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelAW>
                        PeekAW(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelAW>
                        PopAW(size_t index) noexcept override;

        virtual bool    HasW(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelW>
                        PeekW(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelW>
                        PopW(size_t index) noexcept override;

        virtual bool    HasAR(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelAR>
                        PeekAR(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelAR>
                        PopAR(size_t index) noexcept override;

        virtual bool    PushB(size_t index, const AXI::BundleChannelB& channel) noexcept override;

        virtual bool    PushR(size_t index, const AXI::BundleChannelR& channel) noexcept override;

    public:
        bool            SetPortBFIFOSize(size_t index, size_t size = 0) noexcept;
        bool            SetPortRFIFOSize(size_t index, size_t size = 0) noexcept;

    private:
        std::shared_ptr<HadesLink>          link;
        std::shared_ptr<HadesFlitStore>     store;
    };


    // HadesCHIMonitorNull: zero-port CHI monitor interface. The 6-channel
    // PACKING.md map carries no mon_chi_* taps, so CLog.B can only ever write
    // the parameters/topology tags — a valid empty event stream.
    class HadesCHIMonitorNull : public CHI::CHIMonitorInterface {
    public:
        virtual void    TickPostHandshake(uint64_t time) noexcept override { }

        virtual size_t  GetPortCount() const noexcept override    { return 0; }
        virtual std::set<size_t>
                        GetPortIndices() const noexcept override  { return {}; }
        virtual size_t  GetPortMaxIndex() const noexcept override { return 0; }

        virtual bool    HasTXREQ(size_t) const noexcept override  { return false; }
        virtual std::optional<CHI::FiredREQ>
                        PopTXREQ(size_t) noexcept override        { return std::nullopt; }

        virtual bool    HasTXRSP(size_t) const noexcept override  { return false; }
        virtual std::optional<CHI::FiredRSP>
                        PopTXRSP(size_t) noexcept override        { return std::nullopt; }

        virtual bool    HasTXDAT(size_t) const noexcept override  { return false; }
        virtual std::optional<CHI::FiredDAT>
                        PopTXDAT(size_t) noexcept override        { return std::nullopt; }

        virtual bool    HasRXRSP(size_t) const noexcept override  { return false; }
        virtual std::optional<CHI::FiredRSP>
                        PopRXRSP(size_t) noexcept override        { return std::nullopt; }

        virtual bool    HasRXDAT(size_t) const noexcept override  { return false; }
        virtual std::optional<CHI::FiredDAT>
                        PopRXDAT(size_t) noexcept override        { return std::nullopt; }

        virtual bool    HasRXSNP(size_t) const noexcept override  { return false; }
        virtual std::optional<CHI::FiredSNP>
                        PopRXSNP(size_t) noexcept override        { return std::nullopt; }

        virtual bool    HasSN_RXREQ(size_t) const noexcept override { return false; }
        virtual std::optional<CHI::FiredREQ>
                        PopSN_RXREQ(size_t) noexcept override       { return std::nullopt; }

        virtual bool    HasSN_TXRSP(size_t) const noexcept override { return false; }
        virtual std::optional<CHI::FiredRSP>
                        PopSN_TXRSP(size_t) noexcept override       { return std::nullopt; }

        virtual bool    HasSN_TXDAT(size_t) const noexcept override { return false; }
        virtual std::optional<CHI::FiredDAT>
                        PopSN_TXDAT(size_t) noexcept override       { return std::nullopt; }

        virtual bool    HasSN_RXDAT(size_t) const noexcept override { return false; }
        virtual std::optional<CHI::FiredDAT>
                        PopSN_RXDAT(size_t) noexcept override       { return std::nullopt; }
    };
}
