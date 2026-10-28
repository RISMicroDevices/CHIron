//*****************************************************************************
// hades_transport.hpp — TransportIF construction for the cohestra_hades
// layer.
//
//   backend = loopback : hades::LoopbackBackend with a MockFpga configured
//                        from the manifest channel table; the DUT model is
//                        the caller's choice (idle by default — the smoke;
//                        the HADES rehearsal may plug a verilated wrapper's
//                        model instead).
//   backend = jtag     : JtagTcpTransport — a minimal TransportIF over the
//                        jtag_mmio_server.tcl binary batch protocol
//                        (HADES-Fabric scripts/jtag/, TCP port 3122 default).
//   backend = ethernet : hades::EthernetBackend using the manifest's checked
//                        TCP endpoint, timeout, MSS, and queue configuration.
//
// NOTE (Phase J-0): the JTAG bridge RTL (rtl/link/jtag/hades_jtag_bridge.sv)
// contract is ONE BURST = ONE FRAME h2c-side (TLAST on the burst's final
// 16-word group) and frame-exact c2h drains (poll STATUS.c2h_occupancy, then
// size each burst to C2H_WORDS), so multi-segment TOKEN_BATCH_WIDE frames
// pass through end to end on this backend. Control-path MMIO (MAGIC/CTRL/
// descriptors/grants/doorbells) needs none of this.
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
#include <string>
#include <vector>

#include "hades/transport.hpp"

#include "mock_fpga.hpp"    // hades::DutModel (HADES runtime-internal header)

#include "hades_link.hpp"


namespace Cohestra::Hades {

    // Loopback factory: builds the MockFpga config from the manifest channels
    // (the mock validates widths/depths/granules itself) and attaches an idle
    // DUT model unless the caller supplies one (rehearsal hook).
    std::unique_ptr<hades::TransportIF> MakeLoopbackTransport(
        const ManifestInfo& manifest,
        uint64_t            seed,
        hades::DutModel     dutModel = {});

    // Factory on the [hades] backend string. Throws std::runtime_error on
    // unknown backends (xdma remains a later HADES-side milestone).
    std::unique_ptr<hades::TransportIF> MakeTransport(
        const std::string&  backend,
        const std::string&  jtagUrl,
        const ManifestInfo& manifest,
        uint64_t            seed,
        hades::DutModel     dutModel = {});


    // JtagTcpTransport: TransportIF over the length-prefixed binary batch
    // protocol of scripts/jtag/jtag_mmio_server.tcl (v1):
    //   frame: u32 length, u8 payload[length]
    //   request: 0x01 BATCH {u32 n_ops, n_ops x (u8 op, u8 flags, u16 rsv,
    //             u32 addr, u32 word_count[, words])} | 0x02 PING | 0x03 ...
    //   reply:   opcode|0x80, u8 status, u16 n_ops, per-op (u8 op, u8 st,
    //             u16 rsv[, read words])
    // All integers little-endian. One synchronous round trip per poll() batch
    // and per read_mmio() (control path; the hot path is mailbox frames).
    //
    // Address plan (bridge header): 0x0000 fabric CSR (foreign to the bridge,
    // resolved by the top-level decode), 0x1000 bridge registers, 0x2000 H2C
    // frame mailbox (64B-aligned write bursts of 16 words), 0x3000 C2H frame
    // mailbox (reads; poll STATUS.c2h_occupancy first).
    class JtagTcpTransport final : public hades::TransportIF {
    public:
        JtagTcpTransport(const std::string& host, uint16_t port);   // throws std::runtime_error
        ~JtagTcpTransport() override;

        JtagTcpTransport(const JtagTcpTransport&) = delete;
        JtagTcpTransport& operator=(const JtagTcpTransport&) = delete;

    public:
        std::uint32_t   read_mmio (std::uint64_t offset) override;
        void            write_mmio(std::uint64_t offset, std::uint32_t value) override;

        void            push (hades::ChannelId, std::span<const std::byte>) override;
        bool            pull (hades::ChannelId, std::span<std::byte>&) override;
        void            poll () override;
        hades::LinkStatus status() override;

    private:
        struct Op {
            bool                isWrite;
            std::uint32_t       addr;
            std::vector<std::uint32_t> words;   // write payload / read count
        };

        std::vector<std::uint32_t>  RoundTrip(const std::vector<Op>& ops);   // returns per-op read words

        void            LatchError(const std::string& what) noexcept;
        void            FlushOutbox();
        void            RefreshStatus();

    private:
        int             sock        = -1;
        std::string     endpoint;

        std::deque<std::vector<std::byte>>  outbox;     // 64B frames awaiting mailbox space
        std::deque<std::vector<std::byte>>  inbox;      // drained c2h frames
        std::vector<std::byte>              rxHold;     // pull() span target (copied out; the
                                                        // span must outlive the call — the T4
                                                        // use-after-pop lesson)

        std::uint32_t   bridgeStatus = 0;               // 0x100C shadow
        std::uint32_t   fabricStatus = 0;               // 0x008 shadow
        bool            fatal        = false;
        std::string     fatalWhat;
    };
}

#endif
