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
//
// NOTE (Phase J-0): the JTAG bridge RTL (rtl/link/jtag/hades_jtag_bridge.sv)
// carries SINGLE-BEAT 64B frames only; the Oceanus token batches are
// multi-segment TOKEN_BATCH_WIDE frames, so backend=jtag fails loudly on the
// first token batch until the bridge grows multi-beat frames (Phase J-1).
// Control-path MMIO (MAGIC/CTRL/descriptors/grants/doorbells) works today.
//*****************************************************************************
#pragma once

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
    // unknown backends (xdma/ethernet are HADES-side milestones this layer
    // does not drive).
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
