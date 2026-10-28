//*****************************************************************************
// hades_link.hpp — HadesLink: the transport-level window pump of the
// cohestra_hades layer.
//
// HadesLink owns a hades::TransportIF (the frozen M<->B contract,
// runtime/include/hades/transport.hpp) and drives it with REAL per-cycle
// token content (unlike hades::Session, whose T3 implementation stages
// constant all-ones h2c tokens — runtime/src/session.cpp:213-243, "the
// R-layer host-model hook is VP3 scope"). C2H observation in hades::Session
// is the set_token_sink hook (runtime/include/hades/session.hpp:55); here the
// equivalent is the HadesPinSink interface below, fed per unpacked cycle.
//
// Discipline (PROTOCOL.md §4.1a/§5, demo/oceanus_l2/docs/PACKING.md):
//   * exactly one token per DATA channel per target cycle (§2);
//   * h2c tokens are staged in granule-aligned TOKEN_BATCH_WIDE frames
//     (wideBatchGranule, hades/frame.hpp — never recomputed here);
//   * credit ceilings (CREDIT_GRANT) are monotone per channel and lead the
//     staging frontier by ~window_k cycles, capped at max_inflight_windows
//     quanta ahead of the fabric's reported completion;
//   * c2h frames are harvested per poll and unpacked cycle-by-cycle in order,
//     dispatching flits + accept echoes through HadesPinSink.
//
// Cycle mapping: link cycle k is staged exactly once, during harness cycle k
// (HadesLink::DriveCycle), so "cycles" on the CLI/INI mean target cycles, as
// in cohestra_v3. Staging can block (bounded) waiting for credit refresh;
// that is the link being the bottleneck, never a skip.
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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hades/transport.hpp"
#include "hades/frame.hpp"

#include "hades_packing.hpp"


namespace Cohestra::Hades {

    // One DATA channel of the parsed manifest (the subset cohestra_hades
    // consumes; parsed directly with yaml-cpp — see LoadManifestSubset).
    struct ChannelInfo {
        hades::ChannelId    id          = 0;
        std::string         name;
        bool                h2c         = false;
        uint32_t            widthBits   = 0;    // manifest token_width
        uint32_t            widthBytes  = 0;
        uint32_t            fifoDepth   = 0;    // FPGA-side tokens (§6 descriptor)
        uint32_t            granule     = 0;    // staging/emission granule (frame rule)
    };

    // Parsed subset of the demo manifest (demo/oceanus_l2/manifest.yaml).
    struct ManifestInfo {
        std::string                 designName;
        std::string                 sessionName;
        std::string                 backend;        // verbatim string: loopback|jtag|...
        uint32_t                    tickBatch       = 32;
        uint32_t                    maxInflight     = 2;
        uint32_t                    creditReturnDiv = 1024;
        std::vector<ChannelInfo>    channels;

        const ChannelInfo*          Channel(hades::ChannelId id) const noexcept;
    };

    // Minimal direct manifest parse. hades::loadManifest (runtime/src/
    // manifest.cpp) is NOT usable here: its parseBackend rejects
    // "jtag" (the demo's backend) — hades::BackendKind has no Jtag value
    // (runtime/include/hades/transport.hpp:41) — so the boundary layer parses
    // the channel table itself and owns backend selection.
    // Throws std::runtime_error with a readable diagnostic on any violation.
    ManifestInfo LoadManifestSubset(const std::string& path);


    // HadesPinSink: the cycle-boundary callbacks the link drives. Implemented
    // by the flit store behind HadesCCHIInterface / HadesAXISlaveInterface
    // (hades_interface.hpp). Pack writers are pre-zeroed; Unpack readers
    // cover exactly one token. `cycle` is the absolute target-cycle index of
    // the token being packed/unpacked (0-based, from reset).
    class HadesPinSink {
    public:
        virtual ~HadesPinSink() noexcept = default;

    public:
        // h2c pack, one token each, in fixed channel order (1 then 2 then 3).
        virtual void        PackCh1(uint64_t cycle, TokenWriter token) noexcept = 0;
        virtual void        PackCh2(uint64_t cycle, TokenWriter token) noexcept = 0;
        virtual void        PackCh3(uint64_t cycle, TokenWriter token) noexcept = 0;

        // c2h unpack of one cycle, in fixed channel order (4 then 5 then 6).
        virtual void        UnpackCh4(uint64_t cycle, TokenReader token) noexcept = 0;
        virtual void        UnpackCh5(uint64_t cycle, TokenReader token) noexcept = 0;
        virtual void        UnpackCh6(uint64_t cycle, TokenReader token) noexcept = 0;
    };


    struct LinkConfig {
        uint32_t    windowK         = 32;       // [hades] window_k: grant quantum / lead
        uint32_t    maxInflight     = 2;        // manifest max_inflight_windows (W)
        uint64_t    maxQuietPolls   = 2000000;  // wedge guard: polls w/o tick progress
        uint32_t    viewLatency     = 40;       // [hades] view_latency: DUT-cycle view
                                                // offset; staging cycle k stalls until the
                                                // c2h tokens through k+2-viewLatency are
                                                // unpacked (legal stall, never a skip).
                                                // Deadlock floor: maxH2CGranule +
                                                // maxC2HGranule (clamped with a warning).
        bool        flushBridgeOnReset = false; // jtag backend: flush the JTAG bridge
                                                // mailbox (pre-session vacuous-tick
                                                // flood frames, incl. the intermittent
                                                // all-zero anomaly) right after CTRL.run,
                                                // before the first grant/poll.
        uint32_t    maxBatchFrames = hades::kMaxBatchFrames; // [hades] max_batch_frames:
                                                // transport segment cap for h2c staging
                                                // granules (1 = Phase J-0 single-beat
                                                // JTAG bridge, whose per-64B TLAST maps
                                                // each segment to a whole frame). c2h
                                                // decode is self-describing (arg token
                                                // count) and never capped here.
        uint64_t    freezeCycle = 0;            // opt-in HADES_FREEZE_CYCLE; 0 disables
        bool        freezeFinal = false;        // opt-in HADES_FREEZE_FINAL=1
    };

    class HadesLink {
    public:
        HadesLink(std::unique_ptr<hades::TransportIF> transport,
                  const ManifestInfo& manifest,
                  LinkConfig config,
                  HadesPinSink* sink) noexcept;

        ~HadesLink() = default;

        HadesLink(const HadesLink&) = delete;
        HadesLink& operator=(const HadesLink&) = delete;

    public:
        // §6 descriptor table programming (DD-8), then CTRL.reset, then
        // CTRL.run. Clears every local frontier/queue. The wrapper owns the
        // DUT reset stretch; on loopback the mock resets instantly.
        bool        Reset() noexcept;

        // Cycle-start phase (idempotent per time): poll the link once,
        // harvest c2h frames, unpack every cycle whose three c2h tokens have
        // all landed. No staging happens here (see DriveCycle).
        void        TickCycle(uint64_t time) noexcept;

        // End-of-cycle phase: stage link cycle `cycle` (one token per h2c
        // channel), flush completed granules, extend grants, ring the
        // doorbell, then keep polling (bounded) until the cycle is staged.
        // Returns false when the link has failed or the wedge guard tripped.
        bool        DriveCycle(uint64_t cycle) noexcept;

    public:
        bool                    Failed() const noexcept { return lastError.has_value(); }
        const std::string&      LastError() const noexcept { return *lastError; }

        uint64_t                StagedCycles() const noexcept { return staged; }
        uint64_t                GrantedCycles() const noexcept { return granted; }
        uint64_t                CompletedCycles() const noexcept { return completed; }
        uint64_t                UnpackedCycles() const noexcept { return unpacked; }

        uint64_t                H2CFramesPushed() const noexcept { return h2cFrames; }
        uint64_t                C2HFramesDrained() const noexcept { return c2hFrames; }

        const ManifestInfo&     Manifest() const noexcept { return manifest; }
        const ChannelInfo*      Channel(hades::ChannelId id) const noexcept { return manifest.Channel(id); }

    public:
        // Bounded drain used once at end-of-run: over-stage a spill of bubble
        // cycles up to the h2c granule LCM so every staged granule flushes
        // (§4.1a: partial batches never flush), then poll until the fabric
        // reports completion of every staged cycle (or the wedge guard
        // trips). c2h tails inside an unfilled emission granule stay behind
        // per the same contract — logged, not an error.
        bool        FinalDrain() noexcept;

        // Absolute cycle ceiling for the grant pump (0 = unlimited). When
        // set, CREDIT_GRANT ceilings clamp to finalTarget-1 so the fabric can
        // never tick past the harness's intended run length (the session.cpp
        // run(n) clamp, applied incrementally).
        void        SetFinalTarget(uint64_t target) noexcept { finalTarget = target; }

    private:
        void        Fail(std::string message) noexcept;

        void        StageCycle() noexcept;                  // pack cycle `staged` + maybe flush
        void        FlushChannel(size_t index) noexcept;
        void        PumpGrants() noexcept;
        void        PushGrant(hades::ChannelId c, uint32_t ceiling) noexcept;
        void        PollHarvest() noexcept;                 // one poll + drain + unpack
        void        HandleFrame(std::span<const std::byte> frame) noexcept;
        void        UnpackCycles() noexcept;
        void        NoteProgress() noexcept;

        std::uint64_t ReadTick64() noexcept;
        bool        ConfigureFreeze() noexcept;
        void        MaybeFreeze(uint64_t cycle, bool final) noexcept;

    private:
        std::unique_ptr<hades::TransportIF> link;
        ManifestInfo                        manifest;
        LinkConfig                          config;
        bool                                freezeConfigured = false;
        HadesPinSink*                       sink;

        // per-channel staging state (index parallel to manifest.channels;
        // only h2c entries are populated)
        struct Staging {
            std::vector<std::byte>  buffer;     // granule * widthBytes, zeroed on (re)start
            uint32_t                fill    = 0;  // tokens accumulated in the buffer
        };

        // c2h received-token byte stream (contiguous; head cursor + compaction)
        struct C2HStream {
            std::vector<std::byte>  bytes;
            size_t                  head = 0;

            size_t              Available(size_t wb) const noexcept
            { return (bytes.size() - head) / wb; }

            const std::byte*    NextToken() const noexcept
            { return bytes.data() + head; }

            void                Consume(size_t wb) noexcept
            {
                head += wb;
                if (head == bytes.size()) { bytes.clear(); head = 0; }
                else if (head >= 4096 && head >= bytes.size() / 2)
                { bytes.erase(bytes.begin(), bytes.begin() + head); head = 0; }
            }
        };

        std::vector<Staging>                staging;        // h2c channels only
        std::vector<C2HStream>              c2hStreams;     // c2h channels only, raw token bytes

        // frontiers (target cycles since reset)
        uint64_t    staged      = 0;    // cycles packed into h2c tokens
        uint64_t    granted     = 0;    // exclusive grant end on every DATA channel
        uint64_t    completed   = 0;    // fabric tick (CSR tick64 / TICK_ACK hwm+1)
        uint64_t    unpacked    = 0;    // cycles whose c2h bundle is fully consumed
        uint64_t    ackHwm      = 0;
        bool        ackSeen     = false;

        // grant cap: W * window_k cycles ahead of completion, but never less
        // than (largest h2c granule + window_k) — below that, small window_k
        // (lockstep debug) would cap staging under the first granule boundary
        // and wedge: no granule flushes, no fabric tick, no credit refresh
        uint64_t    grantLead   = 0;

        // absolute run ceiling for the grant clamp (0 = unlimited)
        uint64_t    finalTarget = 0;

        // bubble cycles over-staged at FinalDrain past `staged` (spill, the
        // session.cpp "spill stays contiguous" pattern)
        uint64_t    spill       = 0;

        hades::CreditMonotoneCheck  mono;   // §5.2 monotonicity guards

        uint64_t    lastTickTime    = 0;
        bool        lastTickValid   = false;

        uint64_t    h2cFrames   = 0;
        uint64_t    c2hFrames   = 0;

        std::optional<std::string>  lastError;

        bool        running     = false;    // CTRL.run written
    };
}

#endif
