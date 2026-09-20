#pragma once

#include <optional>
#include <set>

#include "../spec/cchi_protocol_flits.hpp"


namespace Cohestra {

    // TagAlias width of the common Type-1 flit configuration (0 = field absent,
    // legacy behavior). Overridable at build time (cohestra_v3 CMake knob
    // COHESTRA_V3_TAGALIAS_WIDTH); all C++ TagAlias access must be guarded by
    // Flits::REQ<...>::hasTagAlias since width 0 makes tagalias_t std::monostate.
#ifndef COHESTRA_TAGALIAS_WIDTH
#define COHESTRA_TAGALIAS_WIDTH 0
#endif

    using CommonFlitConfigurationType1 = CCHI::FlitConfiguration<CCHI::ComponentType::TYPE_1,
        7, 8, 5, 5, 4, 256, COHESTRA_TAGALIAS_WIDTH, true, true>;
    using CommonFlitConfigurationType2 = CCHI::FlitConfiguration<CCHI::ComponentType::TYPE_2>;
    using CommonFlitConfigurationType3 = CCHI::FlitConfiguration<CCHI::ComponentType::TYPE_3>;
    using CommonFlitConfigurationType4 = CCHI::FlitConfiguration<CCHI::ComponentType::TYPE_4>;
    using CommonFlitConfigurationType5 = CCHI::FlitConfiguration<CCHI::ComponentType::TYPE_5>;

    class CCHIInterface {
    public:
        virtual ~CCHIInterface() noexcept = default;

    public:
        // Two-phase channel tick contract:
        //   TickPreHandshake  — cycle start (before the clock=0 eval): drive
        //                       DUT-facing pins from the buffered channels.
        //   TickPostHandshake — mid-cycle (after the clock=0 eval, before the
        //                       posedge eval): sample the settled valid/ready
        //                       pairs (exactly what the DUT registers at the
        //                       upcoming posedge) and retire (pop) the
        //                       transferred flits. No pin writes here.
        // The pop must never be keyed on the live pins at cycle start: those
        // reflect the post-posedge state, which diverges from the DUT's edge
        // sample whenever 'ready' rises exactly at the posedge.
        virtual void    TickPreHandshake(uint64_t time) noexcept = 0;
        virtual void    TickPostHandshake(uint64_t time) noexcept { }

    public:
        virtual size_t  GetType1Count() const noexcept = 0;

        virtual std::set<size_t>
                        GetType1Indices() const noexcept = 0;

        virtual size_t  GetType1MaxIndex() const noexcept = 0;

        virtual bool    HasType1SNP(size_t index) const noexcept = 0;
        virtual std::optional<CCHI::Flits::SNP<CommonFlitConfigurationType1>>
                        PeekType1SNP(size_t index) const noexcept = 0;
        virtual std::optional<CCHI::Flits::SNP<CommonFlitConfigurationType1>>
                        PopType1SNP(size_t index) noexcept = 0;

        virtual bool    HasType1DnRSP(size_t index) const noexcept = 0;
        virtual std::optional<CCHI::Flits::DnRSP<CommonFlitConfigurationType1>>
                        PeekType1DnRSP(size_t index) const noexcept = 0;
        virtual std::optional<CCHI::Flits::DnRSP<CommonFlitConfigurationType1>>
                        PopType1DnRSP(size_t index) noexcept = 0;

        virtual bool    HasType1DnDAT(size_t index) const noexcept = 0;
        virtual std::optional<CCHI::Flits::DnDAT<CommonFlitConfigurationType1>>
                        PeekType1DnDAT(size_t index) const noexcept = 0;
        virtual std::optional<CCHI::Flits::DnDAT<CommonFlitConfigurationType1>>
                        PopType1DnDAT(size_t index) noexcept = 0;

        virtual bool    PushType1EVT(size_t index, const CCHI::Flits::EVT<CommonFlitConfigurationType1>& flit) noexcept = 0;

        virtual bool    PushType1REQ(size_t index, const CCHI::Flits::REQ<CommonFlitConfigurationType1>& flit) noexcept = 0;

        virtual bool    PushType1UpRSP(size_t index, const CCHI::Flits::UpRSP<CommonFlitConfigurationType1>& flit) noexcept = 0;

        virtual bool    PushType1UpDAT(size_t index, const CCHI::Flits::UpDAT<CommonFlitConfigurationType1>& flit) noexcept = 0;

    public:
        // TX-channel (upstream -> downstream, DUT-facing) buffer depth control.
        // Depth 0 frees the FIFO: pushes fall through to the unbuffered
        // direct-drive path (see the implementing class for its caveats).
        // Default no-op: interfaces without buffer support ignore resizes.
        virtual bool    SetAllType1EVTBufferSize(size_t bufferSize) noexcept   { return true; }
        virtual bool    SetAllType1REQBufferSize(size_t bufferSize) noexcept   { return true; }
        virtual bool    SetAllType1UpRSPBufferSize(size_t bufferSize) noexcept { return true; }
        virtual bool    SetAllType1UpDATBufferSize(size_t bufferSize) noexcept { return true; }

    public:
        // TODO
    };
}
