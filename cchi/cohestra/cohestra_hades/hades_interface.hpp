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

#if defined(COHESTRA_HADES)
#  if defined(__has_include) && !__has_include(<hades/transport.hpp>)
#    error "cohestra_hades: COHESTRA_HADES defined but HADES-Fabric headers not on the include path (set HADES_FABRIC_ROOT)"
#  endif
#endif

// Self-neutralizing guard: outside this layer's own opt-in build the whole
// file compiles to an empty translation unit. Content is enabled either by
// the opt-in macro (cohestra_hades build, consumers linking the CMake target)
// or simply by having the HADES-Fabric headers on the include path (the
// HADES rehearsal, which links the archive by file path and therefore does
// not inherit the target's PUBLIC defines).
#if defined(COHESTRA_HADES) || __has_include(<hades/transport.hpp>)

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

        // ---- DUT-cycle anchoring (strict replay-determinism, spec §1.4/D2) --
        // viewLatency: a received c2h flit becomes visible to the Instance
        // exactly (tag + viewLatency) harness cycles after its DUT cycle,
        // never at harvest-arrival time. Deadlock floor (README):
        // (largest h2c staging granule - 1) + (largest c2h emission
        // granule - 1) + 2; the link clamps below it with a warning.
        uint32_t    viewLatency = 40;
        // anchorL*: extra DUT cycles beyond (production tick + 1) for the
        // staging slot of a host-generated response flit (UpRSP/UpDAT/axi
        // b/r). Effective stimulus->response DUT-cycle latency is
        // viewLatency + prep + 1 + anchorL* (all deterministic).
        uint32_t    anchorLRsp  = 0;
        uint32_t    anchorLDat  = 0;
        uint32_t    anchorLB    = 0;
        uint32_t    anchorLR    = 0;
    };

    // a drive-side flit plus its assigned staging slot (absolute DUT cycle at
    // which it may first be packed into a token)
    template<class T>
    struct SlottedFlit {
        T           flit;
        uint64_t    slot = 0;
    };

    // a received c2h flit tagged with the DUT cycle its token was unpacked
    // from (exact, transport-independent)
    template<class T>
    struct TaggedFlit {
        T           flit;
        uint64_t    tag = 0;
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
        // ---- harness-tick bookkeeping (DUT-cycle anchoring) -----------------
        // the harness tick index (== staged-cycle frontier, 1:1); the facades
        // call this from their TickPreHandshake with the half-cycle time base
        void        NoteCycle(uint64_t time) noexcept { currentCycle = time / 2; }
        uint64_t    CurrentCycle() const noexcept     { return currentCycle; }
        uint32_t    ViewLatency() const noexcept      { return config.viewLatency; }

        enum class DriveChan : size_t { EVT = 0, REQ, UpRSP, UpDAT, B, R };

        // allocate the staging slot of a flit being pushed at the current
        // harness tick: max(tick + 1 + anchorL, prev channel slot + 1) —
        // monotone per channel, deterministic given the anchored view
        uint64_t    AllocSlot(DriveChan chan) noexcept;

        // drive-side pushes (Instance/AXIMemory -> store), slot assigned here
        bool        PushSlottedEVT  (const FlitEVT& flit) noexcept;
        bool        PushSlottedREQ  (const FlitREQ& flit) noexcept;
        bool        PushSlottedUpRSP(const FlitUpRSP& flit) noexcept;
        bool        PushSlottedUpDAT(const FlitUpDAT& flit) noexcept;
        bool        PushSlottedB    (const AXI::BundleChannelB& flit) noexcept;
        bool        PushSlottedR    (const AXI::BundleChannelR& flit) noexcept;

        // gated receive access: a received flit is visible to the Instance
        // only from (tag + viewLatency) on — the DUT-cycle-anchored view,
        // never the harvest-arrival time
        const FlitSNP*              PeekSNP() const noexcept;
        std::optional<FlitSNP>      PopSNP() noexcept;
        const FlitDnRSP*            PeekDnRSP() const noexcept;
        std::optional<FlitDnRSP>    PopDnRSP() noexcept;
        const FlitDnDAT*            PeekDnDAT() const noexcept;
        std::optional<FlitDnDAT>    PopDnDAT() noexcept;

        const AXI::BundleChannelAW* PeekAW() const noexcept;
        std::optional<AXI::BundleChannelAW> PopAW() noexcept;
        const AXI::BundleChannelW*  PeekW() const noexcept;
        std::optional<AXI::BundleChannelW>  PopW() noexcept;
        const AXI::BundleChannelAR* PeekAR() const noexcept;
        std::optional<AXI::BundleChannelAR> PopAR() noexcept;

    public:
        // drive-channel views (CCHI interface facade)
        MirrorFIFO<SlottedFlit<FlitEVT>>&        FifoEVT() noexcept    { return fifoEVT; }
        MirrorFIFO<SlottedFlit<FlitREQ>>&        FifoREQ() noexcept    { return fifoREQ; }
        MirrorFIFO<SlottedFlit<FlitUpRSP>>&      FifoUpRSP() noexcept  { return fifoUpRSP; }
        MirrorFIFO<SlottedFlit<FlitUpDAT>>&      FifoUpDAT() noexcept  { return fifoUpDAT; }

        // AXI slave shadow views
        MirrorFIFO<SlottedFlit<AXI::BundleChannelB>>&    FifoB() noexcept { return fifoB; }
        MirrorFIFO<SlottedFlit<AXI::BundleChannelR>>&    FifoR() noexcept { return fifoR; }

    public:
        // protocol-fault counters (logged by the harness at end of run)
        uint64_t    GetFenceBlockedCount() const noexcept  { return fenceBlocked; }
        uint64_t    GetFieldTruncationCount() const noexcept { return fieldTruncations; }

        // debug/probe accessor (mirror-math validation)
        size_t      DebugInflightREQ() const noexcept { return inflightREQ; }

    protected:
        // retire one drive flit on its accept echo (implementation in the .cpp)
        template<class T>
        void ApplyEcho(MirrorFIFO<SlottedFlit<T>>& fifo, std::deque<uint64_t>& stagedCycles,
                       size_t& inflight, bool readyEcho, uint64_t tokenCycle,
                       const char* what) noexcept;

        // the anchored view gate (tag + viewLatency <= currentCycle)
        template<class T>
        const T* GatedFront(const std::deque<TaggedFlit<T>>& queue) const noexcept;

    private:
        BoundaryConfig config;

        // drive FIFOs (pop on accept echo only), slotted by DUT-cycle anchor
        MirrorFIFO<SlottedFlit<FlitEVT>>             fifoEVT;
        MirrorFIFO<SlottedFlit<FlitREQ>>             fifoREQ;
        MirrorFIFO<SlottedFlit<FlitUpRSP>>           fifoUpRSP;
        MirrorFIFO<SlottedFlit<FlitUpDAT>>           fifoUpDAT;
        MirrorFIFO<SlottedFlit<AXI::BundleChannelB>> fifoB;
        MirrorFIFO<SlottedFlit<AXI::BundleChannelR>> fifoR;

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
        // so these never exert backpressure on the DUT), DUT-cycle tagged
        std::deque<TaggedFlit<FlitSNP>>             queueSNP;
        std::deque<TaggedFlit<FlitDnRSP>>           queueDnRSP;
        std::deque<TaggedFlit<FlitDnDAT>>           queueDnDAT;
        std::deque<TaggedFlit<AXI::BundleChannelAW> > queueAW;
        std::deque<TaggedFlit<AXI::BundleChannelW> >  queueW;
        std::deque<TaggedFlit<AXI::BundleChannelAR> > queueAR;

        // harness-tick index (== staged-cycle frontier) and per-drive-channel
        // slot allocation state
        uint64_t    currentCycle = 0;
        uint64_t    prevSlot[6]  = { 0, 0, 0, 0, 0, 0 };

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

#endif
