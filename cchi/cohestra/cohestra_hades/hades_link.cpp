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

//*****************************************************************************
// hades_link.cpp — HadesLink implementation (contract: hades_link.hpp).
//
// CSR offsets are the PROTOCOL.md §6 BAR0 map (mirrored from
// runtime/src/session.cpp:46-56 and tests/sim/rehearsal_full_chain.cpp:57-70 —
// no public HADES header exports them yet). All FRAME-format constants come
// from hades/frame.hpp and are never recomputed here (the wire ver is mid
// 0x01 -> 0x02 bump; this file compiles against whatever the linked tree
// provides and cross-checks it against the backend's VERSION CSR at Reset()).
//*****************************************************************************
#include "hades_link.hpp"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <stdexcept>
#include <unistd.h>

#include "yaml-cpp/yaml.h"

#include "spdlog/spdlog.h"


namespace {

    // §6 CSR map (BAR0), control-path only.
    constexpr std::uint64_t kCsrMagic       = 0x000;    // RO "HADS"
    constexpr std::uint64_t kCsrVersion     = 0x004;    // RO wire ver (frame.hpp kProtocolVer)
    constexpr std::uint64_t kCsrCtrl        = 0x010;    // WO
    constexpr std::uint64_t kCsrTickLo      = 0x018;    // RO
    constexpr std::uint64_t kCsrTickHi      = 0x01C;    // RO
    constexpr std::uint64_t kCsrDoorbellH2C = 0x040;    // WO (batched doorbell)
    constexpr std::uint64_t kCsrChanDesc    = 0x100;    // RW 32B x 256 entries (DD-8)
    constexpr std::uint64_t kBridgeCtrl     = 0x1008;   // WO (hades_jtag_bridge window;
                                                        // jtag backend only)

    constexpr std::uint32_t kCtrlRun        = 0x1;
    constexpr std::uint32_t kCtrlReset      = 0x4;

    constexpr std::uint32_t kMagicValue     = 0x48414453;   // "HADS"

    // §4.1/§4.1a staging/emission granule: wide channels (wb > 8 B) take the
    // A3 rule from the linked frame.cpp; legacy narrow channels keep the
    // (8 + n*wb) % 64 == 0 search (same rule as runtime/src/session.cpp).
    std::uint32_t stagingGranule(std::uint32_t widthBytes)
    {
        if (widthBytes == 0)
            return 0;

        if (hades::isWideTokenBytes(widthBytes))
            return hades::wideBatchGranule(widthBytes);

        for (std::uint32_t n = 1; n <= hades::kFrameAlign; ++n)
            if ((hades::kFrameHeaderBytes + n * widthBytes) % hades::kFrameAlign == 0)
                return n;

        return 0;
    }
}


namespace Cohestra::Hades {

    const ChannelInfo* ManifestInfo::Channel(hades::ChannelId id) const noexcept
    {
        for (const ChannelInfo& c : channels)
            if (c.id == id)
                return &c;

        return nullptr;
    }

    ManifestInfo LoadManifestSubset(const std::string& path)
    {
        YAML::Node root = YAML::LoadFile(path);     // throws YAML::Exception

        ManifestInfo info;

        if (auto design = root["design"])
            info.designName = design["name"].as<std::string>("");

        if (auto session = root["session"])
            info.sessionName = session["name"].as<std::string>("");

        if (auto transport = root["transport"])
        {
            info.backend        = transport["backend"].as<std::string>("loopback");
            info.tickBatch      = transport["tick_batch"].as<uint32_t>(32);
            info.maxInflight    = transport["max_inflight_windows"].as<uint32_t>(2);
        }

        if (auto obs = root["obs"])
            info.creditReturnDiv = obs["credit_return_div"].as<uint32_t>(1024);

        if (auto channels = root["channels"])
        {
            for (const YAML::Node& node : channels)
            {
                ChannelInfo chan;

                chan.id         = static_cast<hades::ChannelId>(node["chan_id"].as<uint32_t>());
                chan.name       = node["name"].as<std::string>("");
                chan.h2c        = node["dir"].as<std::string>("h2c") == "h2c";
                chan.widthBits  = node["token_width"].as<uint32_t>();
                chan.fifoDepth  = node["fifo_depth"].as<uint32_t>();

                if (chan.id == 0 || chan.id > hades::kChanDataMax)
                    throw std::runtime_error("manifest: channel chan_id " +
                        std::to_string(chan.id) + " out of DATA range (1..191)");

                if (chan.widthBits == 0 || (chan.widthBits % 8) != 0)
                    throw std::runtime_error("manifest: channel " + std::to_string(chan.id) +
                        " token_width must be a nonzero multiple of 8");

                chan.widthBytes = chan.widthBits / 8;

                chan.granule = stagingGranule(chan.widthBytes);

                if (chan.granule == 0)
                    throw std::runtime_error("manifest: channel " + std::to_string(chan.id) +
                        " has no valid staging granule");

                if (chan.fifoDepth < chan.granule)
                    throw std::runtime_error("manifest: channel " + std::to_string(chan.id) +
                        " fifo_depth smaller than its granule");

                info.channels.push_back(std::move(chan));
            }
        }

        if (info.channels.empty())
            throw std::runtime_error("manifest: no channels");

        if (info.tickBatch == 0)
            throw std::runtime_error("manifest: tick_batch must be >= 1");

        return info;
    }


    HadesLink::HadesLink(std::unique_ptr<hades::TransportIF> transport,
                         const ManifestInfo& manifest,
                         LinkConfig config,
                         HadesPinSink* sink) noexcept
        : link      (std::move(transport))
        , manifest  (manifest)
        , config    (config)
        , sink      (sink)
    {
        if (this->config.windowK == 0)
            this->config.windowK = 1;

        // Transport segment cap (Phase J-0 single-beat JTAG bridge = 1):
        // re-slice the h2c staging granules so every staged TOKEN_BATCH_WIDE
        // frame fits the cap — the manifest-loaded granules assume the full
        // A3 budget (kMaxBatchFrames). c2h channels are untouched: their
        // decode is self-describing (arg token count) and the fabric's
        // emission granule is elaborated RTL-side.
        for (ChannelInfo& chan : this->manifest.channels)
            if (chan.h2c && hades::isWideTokenBytes(chan.widthBytes))
                chan.granule = hades::wideBatchGranule(chan.widthBytes,
                                                       this->config.maxBatchFrames);

        uint64_t maxH2CGranule = 1;
        uint64_t maxC2HGranule = 1;

        for (const ChannelInfo& chan : this->manifest.channels)
        {
            // both vectors stay indexed parallel to manifest.channels; each
            // channel uses only its direction's entry
            Staging  st;
            C2HStream stream;

            if (chan.h2c)
            {
                st.buffer.assign(size_t(chan.granule) * chan.widthBytes, std::byte{0});
                maxH2CGranule = std::max<uint64_t>(maxH2CGranule, chan.granule);
            }
            else
            {
                maxC2HGranule = std::max<uint64_t>(maxC2HGranule, chan.granule);
            }

            staging.push_back(std::move(st));
            c2hStreams.push_back(std::move(stream));
        }

        const uint64_t maxGranule = std::max(maxH2CGranule, maxC2HGranule);

        grantLead = std::max<uint64_t>(
            uint64_t(this->config.maxInflight) * this->config.windowK,
            maxGranule + this->config.windowK);

        // view-throttle deadlock floor: at a staging halt the fabric can only
        // reach the worst-aligned flushed h2c granule frontier (staging - (max
        // h2c granule - 1)), and unpack trails that by up to (max c2h granule
        // - 1) — the throttle must clear k + 2 - L against that bound, so
        // L >= maxH2CGranule + maxC2HGranule (README derivation)
        const uint64_t viewFloor = maxH2CGranule + maxC2HGranule;

        if (this->config.viewLatency < viewFloor)
        {
            spdlog::warn("[hades] view_latency = {} is below the deadlock floor "
                "(h2c granule {} + c2h granule {}): clamping to {}.",
                this->config.viewLatency, maxH2CGranule, maxC2HGranule, viewFloor);
            this->config.viewLatency = static_cast<uint32_t>(viewFloor);
        }
    }

    void HadesLink::Fail(std::string message) noexcept
    {
        if (!lastError)
        {
            lastError = std::move(message);
            spdlog::error("HADES link: {}", *lastError);
        }
    }

    bool HadesLink::ConfigureFreeze() noexcept
    {
        config.freezeCycle = 0;
        config.freezeFinal = false;

        if (const char* raw = std::getenv("HADES_FREEZE_CYCLE"))
        {
            try
            {
                size_t used = 0;
                const uint64_t value = std::stoull(raw, &used, 0);
                if (used != std::strlen(raw) || value == 0)
                {
                    Fail("HADES_FREEZE_CYCLE must be a positive target cycle");
                    return false;
                }
                config.freezeCycle = value;
            }
            catch (const std::exception& e)
            {
                Fail(std::string("invalid HADES_FREEZE_CYCLE: ") + e.what());
                return false;
            }
        }

        if (const char* raw = std::getenv("HADES_FREEZE_FINAL"))
        {
            const std::string value(raw);
            if (value != "0" && value != "1")
            {
                Fail("HADES_FREEZE_FINAL must be 0 or 1");
                return false;
            }
            config.freezeFinal = (value == "1");
        }

        if (config.freezeCycle || config.freezeFinal)
            spdlog::warn("HADES freeze diagnostics: cycle={} final={}",
                config.freezeCycle, config.freezeFinal);

        return true;
    }

    bool HadesLink::Reset() noexcept
    {
        if (!freezeConfigured)
        {
            freezeConfigured = true;
            if (!ConfigureFreeze()) return false;
        }

        // §6 CHAN_DESC_TABLE (DD-8): entry words +0x00 = {width[31:16],
        // dir[8], chan_id[7:0]} (must equal the entry index), +0x04 =
        // fifo_depth, +0x14 = flags bit0 enable. Matches the programming the
        // rehearsal applies (tests/sim/rehearsal_full_chain.cpp:57-70); on
        // loopback the descriptors are stored but the mock is configured at
        // construction — programming them anyway keeps the hardware path
        // (where CTRL.reset binds them) identical.
        for (const ChannelInfo& chan : manifest.channels)
        {
            const std::uint64_t base = kCsrChanDesc + 32 * chan.id;

            link->write_mmio(base + 0x00,
                (chan.widthBits << 16) | (uint32_t(chan.h2c ? 0 : 1) << 8) | chan.id);
            link->write_mmio(base + 0x04, chan.fifoDepth);
            link->write_mmio(base + 0x14, 1u);  // enable
        }

        // Identity handshake (§6 MAGIC) + wire-version cross-check: the
        // backend reports the ver it was built with; a mismatch against the
        // frame codec compiled in here must never reach the wire silently.
        if (link->read_mmio(kCsrMagic) != kMagicValue)
        {
            Fail("bad MAGIC on BAR0 — not a HADES link");
            return false;
        }

        if (std::uint32_t ver = link->read_mmio(kCsrVersion); ver != hades::kProtocolVer)
            spdlog::warn("HADES link: backend wire ver {:#04x} != host frame codec ver {:#04x} "
                "(mid-bump tree? frame semantics follow the linked hades/frame.hpp).",
                ver, uint32_t(hades::kProtocolVer));

        link->write_mmio(kCsrCtrl, kCtrlReset);

        // clear every local frontier/queue (token streams are per-channel
        // contiguous from cycle 0 — §9 D2)
        staged = granted = completed = unpacked = 0;
        ackHwm = 0;
        ackSeen = false;
        mono.reset();

        for (Staging& st : staging)
        {
            std::fill(st.buffer.begin(), st.buffer.end(), std::byte{0});
            st.fill = 0;
        }

        for (C2HStream& stream : c2hStreams)
        {
            stream.bytes.clear();
            stream.head = 0;
        }

        lastTickValid = false;

        // re-arm the run bit (the fabric side clears it on reset)
        link->write_mmio(kCsrCtrl, kCtrlRun);

        if (config.flushBridgeOnReset)
        {
            // JTAG bridge only (hades_jtag_bridge): with the session configured
            // and run armed, the fabric emits nothing until the first grant —
            // but the bridge mailbox may still hold pre-session vacuous-tick
            // flood frames (accumulated from program time until the CHAN_DESC
            // writes; including the intermittent all-zero anomaly frames, which
            // fail host-side decode and poison the first drain). Soft-reset the
            // bridge pointers (CTRL bit0 — empties both mailboxes), then clear
            // sticky errors (CTRL bit1). Nothing is staged yet, so this is
            // lossless; in-process backends never see it (config-gated).
            link->write_mmio(kBridgeCtrl, 0x1);
            link->write_mmio(kBridgeCtrl, 0x2);
        }

        running = true;

        spdlog::info("HADES link reset: {} DATA channel(s), window_k={}, W={}.",
            manifest.channels.size(), config.windowK, config.maxInflight);

        return true;
    }

    std::uint64_t HadesLink::ReadTick64() noexcept
    {
        // DD-7 safe 64-bit CSR read (LO->HI->LO, retry if LO changed); on
        // in-process backends the first pass is atomic (same as session.cpp).
        for (unsigned attempt = 0; attempt < 16; ++attempt)
        {
            const std::uint32_t lo1 = link->read_mmio(kCsrTickLo);
            const std::uint32_t hi  = link->read_mmio(kCsrTickHi);
            const std::uint32_t lo2 = link->read_mmio(kCsrTickLo);

            if (lo1 == lo2)
                return (static_cast<std::uint64_t>(hi) << 32) | lo1;
        }

        return link->read_mmio(kCsrTickLo);
    }

    void HadesLink::NoteProgress() noexcept
    {
        if (const std::uint64_t t = ReadTick64(); t > completed)
            completed = t;

        if (ackSeen && ackHwm + 1 > completed)
            completed = ackHwm + 1;
    }

    void HadesLink::PushGrant(hades::ChannelId c, std::uint32_t ceiling) noexcept
    {
        std::vector<std::byte> buf(hades::kFrameAlign, std::byte{0});

        hades::FrameHeader h;
        h.type      = hades::FrameType::CreditGrant;
        h.chan_id   = c;
        h.cycle_tag = ceiling;

        const std::size_t n = hades::encodeFrame(buf, h, std::span<const std::byte>{}, /*pad=*/true);

        if (n != hades::kFrameAlign)
        {
            Fail("CREDIT_GRANT encode failed");
            return;
        }

        if (!mono.observeGrant(c, ceiling))
        {
            Fail("CREDIT_GRANT regression (internal bug)");
            return;
        }

        buf.resize(n);
        link->push(c, buf);
        ++h2cFrames;
    }

    void HadesLink::PumpGrants() noexcept
    {
        // Grant staircase, purely staged-paced: credit leads the staging
        // frontier by grantLead cycles, independent of the fabric's reported
        // completion. The old completed-capped shape let grant frames shift
        // their trace positions under host congestion (strict per-cycle wire
        // identity is byte-compared across pacing). Bounding the pipeline is
        // the view throttle's job instead (staging can never outrun the
        // fabric by more than ~view_latency cycles, which is what keeps the
        // fabric's input FIFO and the mock's over-send check safe).
        // Ceilings are monotone per §5 invariant 2; the final target clamp is
        // unchanged.
        while (granted < staged + grantLead && !Failed())
        {
            std::uint64_t ceiling = granted + config.windowK - 1;   // inclusive

            if (finalTarget && ceiling >= finalTarget)
                ceiling = finalTarget - 1;

            if (ceiling < granted)
                break;  // target fully granted — nothing more to extend

            for (const ChannelInfo& chan : manifest.channels)
                PushGrant(chan.id, static_cast<std::uint32_t>(ceiling));  // DD-1 wrap: out of scope

            granted = ceiling + 1;
        }
    }

    void HadesLink::StageCycle() noexcept
    {
        // Pack one token per h2c channel. The pin sink reads the drive-FIFO
        // state and writes the pre-zeroed token; completed granules flush as
        // TOKEN_BATCH_WIDE frames (the token count rides the header arg —
        // §4.1a/DD-9).
        const uint64_t cycle = staged + spill;

        for (size_t i = 0; i < manifest.channels.size(); ++i)
        {
            const ChannelInfo& chan = manifest.channels[i];

            if (!chan.h2c)
                continue;

            Staging& st = staging[i];

            std::byte* tokenAt = st.buffer.data() + size_t(st.fill) * chan.widthBytes;
            std::memset(tokenAt, 0, chan.widthBytes);

            TokenWriter writer(tokenAt);

            switch (chan.id)    // channel semantics per PACKING.md
            {
                case 1: sink->PackCh1(cycle, writer); break;
                case 2: sink->PackCh2(cycle, writer); break;
                case 3: sink->PackCh3(cycle, writer); break;
                default:
                    Fail("unsupported h2c channel id " + std::to_string(chan.id) +
                        " (the PACKING.md 6-channel map carries 1..3)");
                    return;
            }

            if (++st.fill == chan.granule)
                FlushChannel(i);
        }
    }

    void HadesLink::FlushChannel(size_t index) noexcept
    {
        const ChannelInfo&  chan = manifest.channels[index];
        Staging&            st   = staging[index];

        const std::uint32_t g  = chan.granule;
        const std::uint32_t wb = chan.widthBytes;

        std::span<const std::byte> payload(st.buffer.data(), size_t(g) * wb);

        hades::FrameHeader h;
        h.type      = hades::FrameType::TokenBatchWide; // every demo channel is wide (wb > 8)
        h.chan_id   = chan.id;
        h.cycle_tag = g;    // A3/DD-9: the arg field carries the token count

        std::vector<std::byte> buf(hades::kFrameHeaderBytes + payload.size() + hades::kFrameAlign,
                                   std::byte{0});

        const std::size_t n = hades::encodeFrame(buf, h, payload, /*pad=*/true);
        const std::size_t expect = hades::kFrameAlign * hades::wideBatchFrames(g, wb);

        if (n != expect)
        {
            Fail("TOKEN_BATCH_WIDE encode failed (granule misaligned)");
            return;
        }

        buf.resize(n);
        link->push(chan.id, buf);
        ++h2cFrames;

        st.fill = 0;
        std::fill(st.buffer.begin(), st.buffer.end(), std::byte{0});
    }

    void HadesLink::HandleFrame(std::span<const std::byte> frame) noexcept
    {
        hades::FrameHeader h;
        hades::ErrorCode err = hades::ErrorCode::BadFrame;

        const std::span<const std::byte> payload = hades::decodeFrame(frame, h, err);

        // frames are 64B-aligned, so a successful decode of an empty-payload
        // frame still returns the padding span; an empty span means failure
        if (payload.empty())
        {
            Fail("host-side frame decode failed (BAD_FRAME, code " +
                std::to_string(static_cast<std::uint32_t>(err)) + ")");
            return;
        }

        switch (h.type)
        {
            case hades::FrameType::TickAck:
                if (h.chan_id != hades::kChanCtrl || !mono.observeTickAck(h.cycle_tag))
                {
                    Fail("TICK_ACK regression (CYCLE_TAG_REGRESSION)");
                    return;
                }
                ackSeen = true;
                ackHwm  = h.cycle_tag;
                break;

            case hades::FrameType::CreditReturn:
                // progress bookkeeping is CSR/ack-driven here; the return only
                // confirms the per-channel frontier — nothing to do
                break;

            case hades::FrameType::TokenBatch:
            case hades::FrameType::TokenBatchWide:
            {
                const ChannelInfo* chan = manifest.Channel(h.chan_id);

                if (!chan || chan->h2c)
                {
                    Fail("TOKEN_BATCH on unknown/h2c channel " + std::to_string(h.chan_id));
                    return;
                }

                // Token count: wide batches carry it in the arg field
                // (§4.1a/DD-9); legacy frames are alignment-exact by
                // construction (§4.1). Trailing 64B-alignment padding is
                // excluded either way.
                const std::uint64_t count =
                    (h.type == hades::FrameType::TokenBatchWide)
                        ? h.cycle_tag
                        : payload.size() / chan->widthBytes;

                const std::uint64_t bytes = count * chan->widthBytes;

                if (bytes > payload.size())
                {
                    Fail("TOKEN_BATCH payload shorter than its token count on channel " +
                        std::to_string(h.chan_id));
                    return;
                }

                for (size_t i = 0; i < manifest.channels.size(); ++i)
                    if (!manifest.channels[i].h2c && manifest.channels[i].id == h.chan_id)
                    {
                        C2HStream& stream = c2hStreams[i];

                        // copy out of the ring before further pulls recycle it
                        stream.bytes.insert(stream.bytes.end(),
                            payload.begin(), payload.begin() + bytes);
                        break;
                    }

                break;
            }

            case hades::FrameType::Error:
            {
                std::uint32_t code = payload.size() >= 4
                    ? uint32_t(payload[0])
                        | (uint32_t(payload[1]) << 8)
                        | (uint32_t(payload[2]) << 16)
                        | (uint32_t(payload[3]) << 24)
                    : 0;

                Fail("ERROR frame from fabric, code " + std::to_string(code) +
                    " chan " + std::to_string(h.chan_id) +
                    " cycle " + std::to_string(h.cycle_tag));
                return;
            }

            default:
                // settle round-trips / heartbeat echo / obs: not driven by this layer
                break;
        }
    }

    void HadesLink::UnpackCycles() noexcept
    {
        if (!sink)
            return;

        // Unpack strictly in cycle order; cycle u is consumable once every c2h
        // channel holds token u (one token per channel per cycle — §2).
        for (;;)
        {
            for (size_t i = 0; i < manifest.channels.size(); ++i)
            {
                if (manifest.channels[i].h2c)
                    continue;

                if (c2hStreams[i].Available(manifest.channels[i].widthBytes) == 0)
                    return;     // some channel's token for this cycle is missing
            }

            const uint64_t cycle = unpacked;

            for (size_t i = 0; i < manifest.channels.size(); ++i)
            {
                if (manifest.channels[i].h2c)
                    continue;

                const ChannelInfo& chan = manifest.channels[i];
                C2HStream&         stream = c2hStreams[i];

                TokenReader reader(stream.NextToken());

                switch (chan.id)    // channel semantics per PACKING.md
                {
                    case 4: sink->UnpackCh4(cycle, reader); break;
                    case 5: sink->UnpackCh5(cycle, reader); break;
                    case 6: sink->UnpackCh6(cycle, reader); break;
                    default:
                        Fail("unsupported c2h channel id " + std::to_string(chan.id) +
                            " (the PACKING.md 6-channel map carries 4..6)");
                        return;
                }

                stream.Consume(chan.widthBytes);
            }

            ++unpacked;
        }
    }

    void HadesLink::PollHarvest() noexcept
    {
        link->poll();

        // the ring is a single FIFO across channels: loop until no drain
        // channel yields (head-of-line safe), as session.cpp drainC2H does
        std::span<std::byte> frame;

        for (bool any = true; any && !Failed();)
        {
            any = false;

            if (link->pull(hades::kChanCtrl, frame))
            {
                any = true;
                ++c2hFrames;
                HandleFrame(frame);
            }

            // drain EVERY DATA channel, both directions: TOKEN_BATCHes arrive
            // only on c2h channels, but CREDIT_RETURN (§4.3) is emitted per
            // channel including h2c ones — a frame left on an undrained
            // channel head-of-line blocks the single shared ring
            for (size_t i = 0; i < manifest.channels.size(); ++i)
            {
                while (link->pull(manifest.channels[i].id, frame))
                {
                    any = true;
                    ++c2hFrames;
                    HandleFrame(frame);

                    if (Failed())
                        return;
                }
            }
        }

        UnpackCycles();
        NoteProgress();

        if (link->status().error && !Failed())
            Fail("STATUS.error latched by the fabric");
    }

    void HadesLink::MaybeFreeze(uint64_t cycle, bool finalGrant) noexcept
    {
        if (finalGrant)
        {
            if (!config.freezeFinal) return;
        }
        else if (config.freezeCycle == 0 || config.freezeCycle != cycle)
            return;

        if (staged != cycle)
        {
            Fail("freeze staging mismatch: staged=" + std::to_string(staged) +
                " requested=" + std::to_string(cycle));
            return;
        }

        // Stop only after complete MMIO replies. A read-only diagnostic client
        // may then inspect the idle target while this process is stopped.
        for (uint64_t quiet = 0; !Failed();)
        {
            const uint64_t tick = ReadTick64();
            if (tick == staged) break;
            if (tick > staged)
            {
                Fail("freeze target overshot: tick=" + std::to_string(tick) +
                    " staged=" + std::to_string(staged));
                return;
            }
            PollHarvest();
            if (++quiet > config.maxQuietPolls)
            {
                Fail("freeze drain wedge: tick=" + std::to_string(tick) +
                    " staged=" + std::to_string(staged));
                return;
            }
        }

        const uint64_t tick = ReadTick64();
        NoteProgress();
        if (Failed()) return;
        if (tick != staged || (finalGrant && (granted != staged || completed != staged)))
        {
            Fail("freeze boundary mismatch: tick=" + std::to_string(tick) +
                " staged=" + std::to_string(staged) +
                " granted=" + std::to_string(granted) +
                " completed=" + std::to_string(completed));
            return;
        }

        const char* phase = finalGrant ? "final" : "mid";
        spdlog::warn("HADES_FREEZE_READY phase={} cycle={} staged={} granted={} "
            "completed={} tick={} pid={}", phase, cycle, staged, granted, completed,
            tick, getpid());
        spdlog::default_logger()->flush();

        if (raise(SIGSTOP) != 0)
        {
            Fail("failed to raise SIGSTOP for freeze diagnostic");
            return;
        }

        const uint64_t resumedTick = ReadTick64();
        spdlog::warn("HADES_FREEZE_RESUMED phase={} tick={} pid={}",
            phase, resumedTick, getpid());
        spdlog::default_logger()->flush();

        if (resumedTick != tick)
            Fail("target tick changed across host freeze: before=" +
                std::to_string(tick) + " after=" + std::to_string(resumedTick));
    }

    void HadesLink::TickCycle(uint64_t time) noexcept
    {
        if (lastTickValid && lastTickTime == time)
            return;     // idempotent within the same timestamp (v3 rule)

        lastTickTime  = time;
        lastTickValid = true;

        if (Failed())
            return;

        // ---- DUT-cycle-anchored view throttle (tick level) ------------------
        // Hold this harness tick until the c2h tokens the Instance may consume
        // during it (stimuli tagged <= (cycle+1) - viewLatency, per the
        // delivery gate) are unpacked. Then the Instance's response production
        // ticks are pinned to the stimulus tags whatever the harvest pacing —
        // the stall is legal (the DUT simply waits; never a skip).
        //
        // Deadlock-free (no escape needed): the needed data is produced by the
        // fabric at its tick ~ (cycle - L + maxC2HGranule - 1), which requires
        // only staged input up to that tick — always present, since staged ==
        // cycle here and L >= maxC2HGranule + 2 + (startup quirks). The fabric
        // free-runs through the polls below while this tick waits.
        const uint64_t cycle = time / 2;

        std::uint64_t quiet = 0, markC = completed, markU = unpacked;

        while (cycle + 1 > config.viewLatency &&
               unpacked < cycle + 1 - config.viewLatency &&
               !Failed())
        {
            PollHarvest();

            if (completed > markC || unpacked > markU)
            {
                markC = completed;
                markU = unpacked;
                quiet = 0;
            }
            else if (++quiet > config.maxQuietPolls)
            {
                Fail("view throttle wedge: unpacked=" + std::to_string(unpacked) +
                    " (need " + std::to_string(cycle + 1 - config.viewLatency) +
                    ") at tick " + std::to_string(completed) +
                    " (harness cycle " + std::to_string(cycle) + ")");
            }
        }

        PumpGrants();
        PollHarvest();
        MaybeFreeze(cycle, false);
    }

    bool HadesLink::DriveCycle(uint64_t cycle) noexcept
    {
        if (Failed())
            return false;

        if (staged != cycle)
        {
            Fail("internal staging mis-ordering (staged=" + std::to_string(staged) +
                ", drive=" + std::to_string(cycle) + ")");
            return false;
        }

        std::uint64_t quiet = 0, mark = completed;

        while (staged == cycle && !Failed())
        {
            PumpGrants();

            if (staged < granted)
            {
                StageCycle();
                ++staged;

                // one batched doorbell per staged cycle when anything moved
                // (§6: per-descriptor ringing forbidden; the loopback backend
                // treats the doorbell as a harvest hint)
                link->write_mmio(kCsrDoorbellH2C, 1);
            }
            else
            {
                // credit-blocked: the fabric must tick to free the grant cap
                PollHarvest();

                if (completed > mark)
                {
                    mark  = completed;
                    quiet = 0;
                }
                else if (++quiet > config.maxQuietPolls)
                {
                    Fail("poll budget exhausted at tick " + std::to_string(completed) +
                        " (staging cycle " + std::to_string(cycle) + ") — link wedged");
                }
            }
        }

        return !Failed();
    }

    bool HadesLink::FinalDrain() noexcept
    {
        // Spill: over-stage bubble cycles until every h2c channel's staged
        // token count aligns to its granule — §4.1a partial batches never
        // flush, so without the spill the final <granule h2c tokens of a
        // non-tiling run length would never ship and the fabric could not
        // complete the last staged cycles (the session.cpp "spill stays
        // contiguous for the next run" pattern, applied at teardown).
        std::uint64_t lcm = 1;

        for (const ChannelInfo& chan : manifest.channels)
            if (chan.h2c)
                lcm = std::lcm<std::uint64_t>(lcm, chan.granule);

        const std::uint64_t aligned = (staged + lcm - 1) / lcm * lcm;

        while (staged + spill < aligned && !Failed())
        {
            PumpGrants();   // ceilings already clamp to the final target
            StageCycle();   // packs leftover drive flits, bubbles beyond
            ++spill;

            link->write_mmio(kCsrDoorbellH2C, 1);
        }

        if (spill)
            spdlog::info("HADES link: {} spill cycle(s) over-staged to flush the final "
                "granules (phase-1 no-partial-flush contract).", spill);

        std::uint64_t quiet = 0, mark = completed;

        while (completed < staged && !Failed())
        {
            PollHarvest();

            if (completed > mark)
            {
                mark  = completed;
                quiet = 0;
            }
            else if (++quiet > config.maxQuietPolls)
            {
                Fail("final drain wedge: completed=" + std::to_string(completed) +
                    " staged=" + std::to_string(staged));
            }
        }

        MaybeFreeze(staged, true);

        // drain-quiet tail (session.cpp drainQuiet shape): after the fabric
        // has completed every staged cycle, keep harvesting until the c2h
        // side is quiet for a run of consecutive polls, so every formed frame
        // is delivered before the run ends. Without this the trailing unpack
        // count (and the c2h frame count) depends on harvest pacing.
        for (unsigned quietDrain = 0, total = 0;
             quietDrain < 16 && total < 4096 && !Failed(); ++total)
        {
            const std::uint64_t drainedBefore = c2hFrames;

            PollHarvest();

            if (c2hFrames == drainedBefore) ++quietDrain;
            else                            quietDrain = 0;
        }

        if (unpacked < staged)
            spdlog::info("HADES link: {} trailing cycle(s) left in unfilled c2h emission "
                "granules at end of run (phase-1 no-partial-flush contract).",
                staged - unpacked);

        return !Failed();
    }
}

#endif
