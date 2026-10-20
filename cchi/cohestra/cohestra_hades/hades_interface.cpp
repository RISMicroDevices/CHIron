//*****************************************************************************
// hades_interface.cpp — the Cohestra boundary over a HADES-Fabric link
// (contract: hades_interface.hpp). Field offsets are the PACKING.md tables in
// hades_packing.hpp; flit field widths on the C++ side follow
// CommonFlitConfigurationType1 (cohestra_interface.hpp) — pack masks down to
// the DUT's pin width, unpack zero-extends (a truncation that loses bits is
// counted in fieldTruncations and logged once per run).
//
// The semantics below track the actual wrapper RTL (HADES-Fabric
// demo/oceanus_l2/rtl/hades_oceanus_wrapper.sv + hades_oceanus_{h2c,c2h}_skid.sv):
//
//   * h2c drive: a token's in-band valid PUSHES one new flit into the
//     channel's skid FIFO (depth 2*WINDOW_K); a hold register drives the DUT
//     and holds until dut_valid && dut_ready commits at a ce edge. A flit
//     pushed at edge p is drivable from interval p+2 (no token->hold bypass),
//     fires in interval I_e with p <= e-2, and the ready echo sampled at
//     edge e lands in c2h token e+1 — so token t's echo can retire the oldest
//     staged flit only when that flit was staged at cycle <= t-3
//     (kEchoFireLatency). The echo bit is the RAW dut_ready: it can be high
//     on idle cycles, hence the drivability fence (ApplyEcho).
//   * c2h capture: the wrapper pre-asserts its own DUT-facing ready (= its
//     capture skid not-full; the host-driven ch1 ready tail is consumed but
//     UNUSED), captures dut_valid && dut_ready, and pops at most one flit per
//     tick into the registered emission segment — a c2h token's valid marks
//     exactly one delivered flit, exactly once. Enqueue on valid; no host-side
//     ready qualification exists (or is needed).
//   * reset: the wrapper's stretcher flushes all skid/hold state for
//     RST_CYCLES gated cycles and ignores pushes. Flits staged there would
//     vanish without echo, so the store drives only idle tokens for the first
//     [root] reset_preamble cycles after reset (the bubble window;
//     reset_preamble default 1024 >= 128).
//*****************************************************************************
#include "hades_interface.hpp"

#include <cstring>

#include "spdlog/spdlog.h"


namespace Cohestra::Hades {

    namespace {

        // A flit staged at cycle p is drivable from interval p+2, fires in
        // interval I_e with p <= e-2, and its ready echo lands in c2h token
        // e+1: token t may retire the oldest staged flit only when that flit
        // was staged at cycle <= t - kEchoFireLatency.
        constexpr uint64_t kEchoFireLatency = 3;

        // value masked to `width` bits; returns false when bits were lost
        inline bool FitsWidth(uint64_t value, size_t width) noexcept
        {
            return width >= 64 || (value >> width) == 0;
        }

        template<class TFlit>
        inline uint64_t Field(const TFlit& v) noexcept
        {
            return static_cast<uint64_t>(v);
        }
    }


    HadesFlitStore::HadesFlitStore(BoundaryConfig config) noexcept
        : config(config)
    {
        // v3-compatible default depth 4 is link-hostile (echo lag spans the
        // whole in-flight window): default deep, Instance pre-init resizes
        // from [root] upstream.type1.buffer.* anyway
        fifoEVT  .TryResize(64);
        fifoREQ  .TryResize(64);
        fifoUpRSP.TryResize(64);
        fifoUpDAT.TryResize(64);
        fifoB    .TryResize(64);
        fifoR    .TryResize(64);
    }

    void HadesFlitStore::Clear() noexcept
    {
        fifoEVT  .Clear();  fifoREQ  .Clear();
        fifoUpRSP.Clear();  fifoUpDAT.Clear();
        fifoB    .Clear();  fifoR    .Clear();

        inflightEVT = inflightREQ = inflightUpRSP = 0;
        inflightUpDAT = inflightB = inflightR = 0;

        stagedEVT.clear(); stagedREQ.clear(); stagedUpRSP.clear();
        stagedUpDAT.clear(); stagedB.clear(); stagedR.clear();

        queueSNP.clear(); queueDnRSP.clear(); queueDnDAT.clear();
        queueAW .clear(); queueW     .clear(); queueAR .clear();

        fenceBlocked = fieldTruncations = 0;
    }

    // ---- staging (h2c) ---------------------------------------------------------

    void HadesFlitStore::PackCh1(uint64_t cycle, TokenWriter token) noexcept
    {
        namespace P = PackCh1;

        // ready-drive placeholders [197:192]: the wrapper drives the
        // DUT-facing readies from its own capture-skid occupancy and consumes
        // these bits unused — tied 1 ("credit pre-asserted", PACKING.md)
        token.Set(P::RDY_TXSNP, 1, 1);
        token.Set(P::RDY_TXRSP, 1, 1);
        token.Set(P::RDY_TXDAT, 1, 1);
        token.Set(P::RDY_AW,    1, 1);
        token.Set(P::RDY_W,     1, 1);
        token.Set(P::RDY_AR,    1, 1);

        if (cycle < config.bubbleCycles)
            return;     // post-reset bubble window: idle tokens only

        // rxevt
        if (inflightEVT < fifoEVT.Count() && inflightEVT < config.skidDepth)
        {
            const FlitEVT& flit = *fifoEVT.PeekAt(inflightEVT);
            ++inflightEVT;
            stagedEVT.push_back(cycle);

            token.Set(P::EVT_VALID,    1,  1);
            token.Set(P::EVT_ADDR,     48, Field(flit.Addr));
            token.Set(P::EVT_TRACETAG, 1,  Field(flit.TraceTag));
            token.Set(P::EVT_WAY,      2,  Field(flit.Way));
            token.Set(P::EVT_WAYVALID, 1,  Field(flit.WayValid));
            token.Set(P::EVT_MEMATTR,  1,  Field(flit.MemAttr));
            token.Set(P::EVT_NS,       1,  Field(flit.NS));
            token.Set(P::EVT_OPCODE,   1,  Field(flit.Opcode));
            token.Set(P::EVT_TGTID,    4,  Field(flit.TgtID));
            token.Set(P::EVT_SRCID,    4,  Field(flit.SrcID));
            token.Set(P::EVT_TXNID,    8,  Field(flit.TxnID));
        }

        // rxreq
        if (inflightREQ < fifoREQ.Count() && inflightREQ < config.skidDepth)
        {
            const FlitREQ& flit = *fifoREQ.PeekAt(inflightREQ);
            ++inflightREQ;
            stagedREQ.push_back(cycle);

            token.Set(P::REQ_VALID,    1,  1);
            token.Set(P::REQ_ADDR,     48, Field(flit.Addr));
            token.Set(P::REQ_TRACETAG, 1,  Field(flit.TraceTag));
            token.Set(P::REQ_WAY,      2,  Field(flit.Way));
            token.Set(P::REQ_WAYVALID, 1,  Field(flit.WayValid));
            token.Set(P::REQ_EXPCOMP,  1,  Field(flit.ExpCompData));
            token.Set(P::REQ_EXCL,     1,  Field(flit.Excl));
            token.Set(P::REQ_MEMATTR,  4,  Field(flit.MemAttr));
            token.Set(P::REQ_ORDER,    2,  Field(flit.Order));
            token.Set(P::REQ_NS,       1,  Field(flit.NS));
            token.Set(P::REQ_SIZE,     3,  Field(flit.Size));
            if constexpr (FlitREQ::hasTagAlias)
                token.Set(P::REQ_TAGALIAS, 2, Field(flit.TagAlias));
            token.Set(P::REQ_OPCODE,   6,  Field(flit.Opcode));
            token.Set(P::REQ_TGTID,    4,  Field(flit.TgtID));
            token.Set(P::REQ_SRCID,    4,  Field(flit.SrcID));
            token.Set(P::REQ_TXNID,    8,  Field(flit.TxnID));
        }

        // rxrsp (UpRSP)
        if (inflightUpRSP < fifoUpRSP.Count() && inflightUpRSP < config.skidDepth)
        {
            const FlitUpRSP& flit = *fifoUpRSP.PeekAt(inflightUpRSP);
            ++inflightUpRSP;
            stagedUpRSP.push_back(cycle);

            token.Set(P::RSP_VALID,    1,  1);
            token.Set(P::RSP_TRACETAG, 1,  Field(flit.TraceTag));
            token.Set(P::RSP_RESP,     3,  Field(flit.Resp));
            token.Set(P::RSP_RESPERR,  2,  Field(flit.RespErr));
            token.Set(P::RSP_OPCODE,   1,  Field(flit.Opcode));
            token.Set(P::RSP_TGTID,    4,  Field(flit.TgtID));
            token.Set(P::RSP_SRCID,    4,  Field(flit.SrcID));
            token.Set(P::RSP_TXNID,    8,  Field(flit.TxnID));
        }

        // axi_m0_b
        if (inflightB < fifoB.Count() && inflightB < config.skidDepth)
        {
            const AXI::BundleChannelB& flit = *fifoB.PeekAt(inflightB);
            ++inflightB;
            stagedB.push_back(cycle);

            token.Set(P::B_VALID, 1, 1);
            token.Set(P::B_ID,    4, flit.Id);
            token.Set(P::B_RESP,  2, static_cast<uint8_t>(flit.Resp));
        }
    }

    void HadesFlitStore::PackCh2(uint64_t cycle, TokenWriter token) noexcept
    {
        namespace P = PackCh2;

        if (cycle < config.bubbleCycles)
            return;

        // rxdat (UpDAT)
        if (inflightUpDAT < fifoUpDAT.Count() && inflightUpDAT < config.skidDepth)
        {
            const FlitUpDAT& flit = *fifoUpDAT.PeekAt(inflightUpDAT);
            ++inflightUpDAT;
            stagedUpDAT.push_back(cycle);

            token.Set(P::DAT_VALID,    1,   1);
            token.Set(P::DAT_BE,       32,  Field(flit.BE));
            token.SetWide(P::DAT_DATA,      flit.Data, 256);
            token.Set(P::DAT_TRACETAG, 1,   Field(flit.TraceTag));
            token.Set(P::DAT_DATAID,   2,   Field(flit.DataID));
            token.Set(P::DAT_RESP,     3,   Field(flit.Resp));
            token.Set(P::DAT_RESPERR,  2,   Field(flit.RespErr));
            token.Set(P::DAT_OPCODE,   2,   Field(flit.Opcode));
            token.Set(P::DAT_TGTID,    4,   Field(flit.TgtID));
            token.Set(P::DAT_SRCID,    4,   Field(flit.SrcID));
            token.Set(P::DAT_TXNID,    8,   Field(flit.TxnID));
        }
    }

    void HadesFlitStore::PackCh3(uint64_t cycle, TokenWriter token) noexcept
    {
        namespace P = PackCh3;

        if (cycle < config.bubbleCycles)
            return;

        // axi_m0_r
        if (inflightR < fifoR.Count() && inflightR < config.skidDepth)
        {
            const AXI::BundleChannelR& flit = *fifoR.PeekAt(inflightR);
            ++inflightR;
            stagedR.push_back(cycle);

            token.Set(P::R_VALID, 1, 1);
            token.Set(P::R_ID,    4, flit.Id);

            uint64_t data[4] = { 0, 0, 0, 0 };
            for (size_t i = 0; i < 4 && i < flit.Data.size(); ++i)
                data[i] = flit.Data[i];

            token.SetWide(P::R_DATA, data, 256);
            token.Set(P::R_RESP, 2, static_cast<uint8_t>(flit.Resp));
            token.Set(P::R_LAST, 1, flit.Last);
        }
    }

    // ---- retirement + receive (c2h) ---------------------------------------------

    template<class T>
    void HadesFlitStore::ApplyEcho(MirrorFIFO<T>& fifo, std::deque<uint64_t>& stagedCycles,
                                   size_t& inflight, bool readyEcho, uint64_t tokenCycle,
                                   const char* what) noexcept
    {
        if (!readyEcho)
            return;

        if (stagedCycles.empty() ||
            stagedCycles.front() + kEchoFireLatency > tokenCycle)
        {
            // dut_ready high while no staged flit is drivable at the sampled
            // edge (nothing, or nothing yet, being driven): an idle cycle's
            // ready pin, NOT an accept — do not pop. Expected constantly:
            // the wrapper samples the raw ready pin every tick.
            ++fenceBlocked;
            return;
        }

        // accept: the wrapper committed the oldest staged flit at the sampled
        // edge — retire it (and only it: one hold register, one fire per edge)
        fifo.Pop();
        stagedCycles.pop_front();
        --inflight;
    }

    void HadesFlitStore::UnpackCh4(uint64_t cycle, TokenReader token) noexcept
    {
        namespace P = PackCh4;

        // txsnp — valid marks exactly one captured flit, delivered once
        if (token.Get(P::SNP_VALID, 1))
        {
            FlitSNP flit;
            flit.Addr     = token.Get(P::SNP_ADDR,    45);
            flit.TraceTag = token.Get(P::SNP_TRACETAG, 1);
            flit.NS       = token.Get(P::SNP_NS,       1);
            flit.Opcode   = token.Get(P::SNP_OPCODE,   2);
            flit.TgtID    = token.Get(P::SNP_TGTID,    4);
            flit.SrcID    = token.Get(P::SNP_SRCID,    4);
            flit.TxnID    = token.Get(P::SNP_TXNID,    8);
            // [2:1] alias is a DUT-side extra with no
            // CommonFlitConfigurationType1 field — read and dropped

            queueSNP.push_back(flit);
        }

        // txrsp
        if (token.Get(P::RSP_VALID, 1))
        {
            FlitDnRSP flit;
            flit.TraceTag = token.Get(P::RSP_TRACETAG, 1);
            flit.Way      = token.Get(P::RSP_WAY,      2);
            flit.WayValid = token.Get(P::RSP_WAYVALID, 1);
            flit.CBusy    = token.Get(P::RSP_CBUSY,    3);
            flit.Resp     = token.Get(P::RSP_RESP,     3);
            flit.RespErr  = token.Get(P::RSP_RESPERR,  2);
            flit.Opcode   = token.Get(P::RSP_OPCODE,   3);
            flit.DBID     = token.Get(P::RSP_DBID,     8);
            flit.TgtID    = token.Get(P::RSP_TGTID,    4);
            flit.SrcID    = token.Get(P::RSP_SRCID,    4);
            flit.TxnID    = token.Get(P::RSP_TXNID,    8);

            queueDnRSP.push_back(flit);
        }

        // axi_m0_aw
        if (token.Get(P::AW_VALID, 1))
        {
            AXI::BundleChannelAW chan;
            chan.Id    = token.Get(P::AW_ID,    4);
            chan.Addr  = token.Get(P::AW_ADDR,  32);
            chan.Len   = token.Get(P::AW_LEN,   8);
            chan.Size  = token.Get(P::AW_SIZE,  3);

            const uint64_t burst = token.Get(P::AW_BURST, 2);
            chan.Burst = burst == 0 ? AXI::Burst::FIXED
                       : burst == 1 ? AXI::Burst::INCR : AXI::Burst::WRAP;

            queueAW.push_back(std::move(chan));
        }

        // axi_m0_ar
        if (token.Get(P::AR_VALID, 1))
        {
            AXI::BundleChannelAR chan;
            chan.Id    = token.Get(P::AR_ID,    4);
            chan.Addr  = token.Get(P::AR_ADDR,  32);
            chan.Len   = token.Get(P::AR_LEN,   8);
            chan.Size  = token.Get(P::AR_SIZE,  3);

            const uint64_t burst = token.Get(P::AR_BURST, 2);
            chan.Burst = burst == 0 ? AXI::Burst::FIXED
                       : burst == 1 ? AXI::Burst::INCR : AXI::Burst::WRAP;

            queueAR.push_back(std::move(chan));
        }

        // accept echo tail [213:208] — the ONLY retirement of the drive FIFOs.
        // The bits are the RAW DUT ready pins sampled at the tick edge; the
        // drivability fence in ApplyEcho reconstructs valid&&ready from it.
        ApplyEcho(fifoEVT,   stagedEVT,   inflightEVT,   token.Get(P::ECHO_EVT, 1) != 0, cycle, "rxevt");
        ApplyEcho(fifoREQ,   stagedREQ,   inflightREQ,   token.Get(P::ECHO_REQ, 1) != 0, cycle, "rxreq");
        ApplyEcho(fifoUpRSP, stagedUpRSP, inflightUpRSP, token.Get(P::ECHO_RSP, 1) != 0, cycle, "rxrsp");
        ApplyEcho(fifoUpDAT, stagedUpDAT, inflightUpDAT, token.Get(P::ECHO_DAT, 1) != 0, cycle, "rxdat");
        ApplyEcho(fifoB,     stagedB,     inflightB,     token.Get(P::ECHO_B,   1) != 0, cycle, "axi_m0_b");
        ApplyEcho(fifoR,     stagedR,     inflightR,     token.Get(P::ECHO_R,   1) != 0, cycle, "axi_m0_r");
    }

    void HadesFlitStore::UnpackCh5(uint64_t cycle, TokenReader token) noexcept
    {
        namespace P = PackCh5;

        // txdat
        if (token.Get(P::DAT_VALID, 1))
        {
            FlitDnDAT flit;
            token.GetWide(P::DAT_DATA, flit.Data, 256);
            flit.TraceTag   = token.Get(P::DAT_TRACETAG, 1);
            flit.Way        = token.Get(P::DAT_WAY,      2);
            flit.WayValid   = token.Get(P::DAT_WAYVALID, 1);
            flit.CBusy      = token.Get(P::DAT_CBUSY,    3);
            flit.DataSource = token.Get(P::DAT_DATASRC,  5);

            const uint64_t dataId = token.Get(P::DAT_DATAID, 2);
            flit.DataID = dataId;
            if (!FitsWidth(dataId, FlitDnDAT::DATAID_WIDTH))
                ++fieldTruncations;

            flit.Resp       = token.Get(P::DAT_RESP,     3);
            flit.RespErr    = token.Get(P::DAT_RESPERR,  2);
            flit.Opcode     = token.Get(P::DAT_OPCODE,   1);
            flit.DBID       = token.Get(P::DAT_DBID,     8);
            flit.TgtID      = token.Get(P::DAT_TGTID,    4);
            flit.SrcID      = token.Get(P::DAT_SRCID,    4);
            flit.TxnID      = token.Get(P::DAT_TXNID,    8);

            queueDnDAT.push_back(flit);
        }
    }

    void HadesFlitStore::UnpackCh6(uint64_t cycle, TokenReader token) noexcept
    {
        namespace P = PackCh6;

        // axi_m0_w
        if (token.Get(P::W_VALID, 1))
        {
            AXI::BundleChannelW chan;

            chan.Data.resize(4);
            token.GetWide(P::W_DATA, chan.Data.data(), 256);

            chan.Strb.assign(1, token.Get(P::W_STRB, 32));
            chan.Last = token.Get(P::W_LAST, 1);

            queueW.push_back(std::move(chan));
        }
    }


    // ---- HadesCCHIInterface ------------------------------------------------------

    HadesCCHIInterface::HadesCCHIInterface(std::shared_ptr<HadesLink> link,
                                           std::shared_ptr<HadesFlitStore> store) noexcept
        : link(std::move(link)), store(std::move(store))
    { }

    void HadesCCHIInterface::TickPreHandshake(uint64_t time) noexcept
    {
        // drive phase over the link: poll/harvest/unpack (the cycle's own
        // token staging happens in the harness's DriveCycle tail, after the
        // Instance's pushes of this tick have landed)
        link->TickCycle(time);
    }

    void HadesCCHIInterface::TickPostHandshake(uint64_t time) noexcept
    {
        // No pin sampling here (contrast cohestra_v3/interface.hpp:784-797):
        // the settled valid&&ready pairs come back on the link as the ch4
        // ready echo and retire flits at unpack time, so this phase is
        // deliberately empty. Kept an override for the contract's shape.
    }

    size_t HadesCCHIInterface::GetType1Count() const noexcept
    {
        return 1;   // the PACKING.md boundary is exactly t1p0
    }

    std::set<size_t> HadesCCHIInterface::GetType1Indices() const noexcept
    {
        return { 0 };
    }

    size_t HadesCCHIInterface::GetType1MaxIndex() const noexcept
    {
        return 0;
    }

    bool HadesCCHIInterface::HasType1SNP(size_t index) const noexcept
    {
        return index == 0 && !store->QueueSNP().empty();
    }

    std::optional<CCHI::Flits::SNP<CommonFlitConfigurationType1>>
        HadesCCHIInterface::PeekType1SNP(size_t index) const noexcept
    {
        if (index != 0 || store->QueueSNP().empty())
            return std::nullopt;

        return { store->QueueSNP().front() };
    }

    std::optional<CCHI::Flits::SNP<CommonFlitConfigurationType1>>
        HadesCCHIInterface::PopType1SNP(size_t index) noexcept
    {
        if (index != 0 || store->QueueSNP().empty())
            return std::nullopt;

        auto flit = store->QueueSNP().front();
        store->QueueSNP().pop_front();
        return { flit };
    }

    bool HadesCCHIInterface::HasType1DnRSP(size_t index) const noexcept
    {
        return index == 0 && !store->QueueDnRSP().empty();
    }

    std::optional<CCHI::Flits::DnRSP<CommonFlitConfigurationType1>>
        HadesCCHIInterface::PeekType1DnRSP(size_t index) const noexcept
    {
        if (index != 0 || store->QueueDnRSP().empty())
            return std::nullopt;

        return { store->QueueDnRSP().front() };
    }

    std::optional<CCHI::Flits::DnRSP<CommonFlitConfigurationType1>>
        HadesCCHIInterface::PopType1DnRSP(size_t index) noexcept
    {
        if (index != 0 || store->QueueDnRSP().empty())
            return std::nullopt;

        auto flit = store->QueueDnRSP().front();
        store->QueueDnRSP().pop_front();
        return { flit };
    }

    bool HadesCCHIInterface::HasType1DnDAT(size_t index) const noexcept
    {
        return index == 0 && !store->QueueDnDAT().empty();
    }

    std::optional<CCHI::Flits::DnDAT<CommonFlitConfigurationType1>>
        HadesCCHIInterface::PeekType1DnDAT(size_t index) const noexcept
    {
        if (index != 0 || store->QueueDnDAT().empty())
            return std::nullopt;

        return { store->QueueDnDAT().front() };
    }

    std::optional<CCHI::Flits::DnDAT<CommonFlitConfigurationType1>>
        HadesCCHIInterface::PopType1DnDAT(size_t index) noexcept
    {
        if (index != 0 || store->QueueDnDAT().empty())
            return std::nullopt;

        auto flit = store->QueueDnDAT().front();
        store->QueueDnDAT().pop_front();
        return { flit };
    }

    bool HadesCCHIInterface::PushType1EVT(size_t index, const CCHI::Flits::EVT<CommonFlitConfigurationType1>& flit) noexcept
    {
        return index == 0 && store->FifoEVT().Push(flit);
    }

    bool HadesCCHIInterface::PushType1REQ(size_t index, const CCHI::Flits::REQ<CommonFlitConfigurationType1>& flit) noexcept
    {
        return index == 0 && store->FifoREQ().Push(flit);
    }

    bool HadesCCHIInterface::PushType1UpRSP(size_t index, const CCHI::Flits::UpRSP<CommonFlitConfigurationType1>& flit) noexcept
    {
        return index == 0 && store->FifoUpRSP().Push(flit);
    }

    bool HadesCCHIInterface::PushType1UpDAT(size_t index, const CCHI::Flits::UpDAT<CommonFlitConfigurationType1>& flit) noexcept
    {
        return index == 0 && store->FifoUpDAT().Push(flit);
    }

    bool HadesCCHIInterface::SetType1EVTBufferSize(size_t index, size_t bufferSize) noexcept
    {
        return index == 0 && store->FifoEVT().TryResize(bufferSize);
    }

    bool HadesCCHIInterface::SetType1REQBufferSize(size_t index, size_t bufferSize) noexcept
    {
        return index == 0 && store->FifoREQ().TryResize(bufferSize);
    }

    bool HadesCCHIInterface::SetType1UpRSPBufferSize(size_t index, size_t bufferSize) noexcept
    {
        return index == 0 && store->FifoUpRSP().TryResize(bufferSize);
    }

    bool HadesCCHIInterface::SetType1UpDATBufferSize(size_t index, size_t bufferSize) noexcept
    {
        return index == 0 && store->FifoUpDAT().TryResize(bufferSize);
    }

    bool HadesCCHIInterface::SetAllType1EVTBufferSize(size_t bufferSize) noexcept
    {
        return SetType1EVTBufferSize(0, bufferSize);
    }

    bool HadesCCHIInterface::SetAllType1REQBufferSize(size_t bufferSize) noexcept
    {
        return SetType1REQBufferSize(0, bufferSize);
    }

    bool HadesCCHIInterface::SetAllType1UpRSPBufferSize(size_t bufferSize) noexcept
    {
        return SetType1UpRSPBufferSize(0, bufferSize);
    }

    bool HadesCCHIInterface::SetAllType1UpDATBufferSize(size_t bufferSize) noexcept
    {
        return SetType1UpDATBufferSize(0, bufferSize);
    }


    // ---- HadesAXISlaveInterface --------------------------------------------------

    HadesAXISlaveInterface::HadesAXISlaveInterface(std::shared_ptr<HadesLink> link,
                                                   std::shared_ptr<HadesFlitStore> store) noexcept
        : link(std::move(link)), store(std::move(store))
    { }

    void HadesAXISlaveInterface::TickPreHandshake(uint64_t time) noexcept
    {
        link->TickCycle(time);  // idempotent within the same timestamp
    }

    void HadesAXISlaveInterface::TickPostHandshake(uint64_t time) noexcept
    {
        // as HadesCCHIInterface::TickPostHandshake: retirement rides the echo
    }

    size_t HadesAXISlaveInterface::GetPortCount() const noexcept
    {
        return 1;
    }

    std::set<size_t> HadesAXISlaveInterface::GetPortIndices() const noexcept
    {
        return { 0 };
    }

    size_t HadesAXISlaveInterface::GetPortMaxIndex() const noexcept
    {
        return 0;
    }

    size_t HadesAXISlaveInterface::GetPortDataBits(size_t index) const noexcept
    {
        return index == 0 ? 256 : 0;    // axi_m0 is 256-bit (PACKING.md ch2/3/5/6)
    }

    bool HadesAXISlaveInterface::HasAW(size_t index) const noexcept
    {
        return index == 0 && !store->QueueAW().empty();
    }

    std::optional<AXI::BundleChannelAW> HadesAXISlaveInterface::PeekAW(size_t index) const noexcept
    {
        if (index != 0 || store->QueueAW().empty())
            return std::nullopt;

        return { store->QueueAW().front() };
    }

    std::optional<AXI::BundleChannelAW> HadesAXISlaveInterface::PopAW(size_t index) noexcept
    {
        if (index != 0 || store->QueueAW().empty())
            return std::nullopt;

        auto chan = std::move(store->QueueAW().front());
        store->QueueAW().pop_front();
        return { std::move(chan) };
    }

    bool HadesAXISlaveInterface::HasW(size_t index) const noexcept
    {
        return index == 0 && !store->QueueW().empty();
    }

    std::optional<AXI::BundleChannelW> HadesAXISlaveInterface::PeekW(size_t index) const noexcept
    {
        if (index != 0 || store->QueueW().empty())
            return std::nullopt;

        return { store->QueueW().front() };
    }

    std::optional<AXI::BundleChannelW> HadesAXISlaveInterface::PopW(size_t index) noexcept
    {
        if (index != 0 || store->QueueW().empty())
            return std::nullopt;

        auto chan = std::move(store->QueueW().front());
        store->QueueW().pop_front();
        return { std::move(chan) };
    }

    bool HadesAXISlaveInterface::HasAR(size_t index) const noexcept
    {
        return index == 0 && !store->QueueAR().empty();
    }

    std::optional<AXI::BundleChannelAR> HadesAXISlaveInterface::PeekAR(size_t index) const noexcept
    {
        if (index != 0 || store->QueueAR().empty())
            return std::nullopt;

        return { store->QueueAR().front() };
    }

    std::optional<AXI::BundleChannelAR> HadesAXISlaveInterface::PopAR(size_t index) noexcept
    {
        if (index != 0 || store->QueueAR().empty())
            return std::nullopt;

        auto chan = std::move(store->QueueAR().front());
        store->QueueAR().pop_front();
        return { std::move(chan) };
    }

    bool HadesAXISlaveInterface::PushB(size_t index, const AXI::BundleChannelB& channel) noexcept
    {
        return index == 0 && store->FifoB().Push(channel);
    }

    bool HadesAXISlaveInterface::PushR(size_t index, const AXI::BundleChannelR& channel) noexcept
    {
        return index == 0 && store->FifoR().Push(channel);
    }

    bool HadesAXISlaveInterface::SetPortBFIFOSize(size_t index, size_t size) noexcept
    {
        return index == 0 && store->FifoB().TryResize(size);
    }

    bool HadesAXISlaveInterface::SetPortRFIFOSize(size_t index, size_t size) noexcept
    {
        return index == 0 && store->FifoR().TryResize(size);
    }
}
