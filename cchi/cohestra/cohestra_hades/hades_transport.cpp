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
// hades_transport.cpp — TransportIF construction (contract:
// hades_transport.hpp).
//*****************************************************************************
#include "hades_transport.hpp"

#include <stdexcept>

#include "loopback_backend.hpp"     // hades::LoopbackBackend (runtime-internal)

// POSIX sockets (the JTAG MMIO server is a TCP bridge; WSL2/Linux host side)
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include "spdlog/spdlog.h"


namespace Cohestra::Hades {

    std::unique_ptr<hades::TransportIF> MakeLoopbackTransport(
        const ManifestInfo& manifest,
        uint64_t            seed,
        hades::DutModel     dutModel)
    {
        hades::MockFpga::Config cfg;

        for (const ChannelInfo& chan : manifest.channels)
        {
            hades::MockFpga::ChannelConfig cc;
            cc.chan_id           = chan.id;
            cc.h2c               = chan.h2c;
            cc.token_width_bits  = static_cast<std::uint16_t>(chan.widthBits);
            cc.fifo_depth_tokens = chan.fifoDepth;
            cfg.channels.push_back(cc);
        }

        cfg.tick_batch        = manifest.tickBatch;
        cfg.credit_return_div = manifest.creditReturnDiv;
        cfg.dut               = std::move(dutModel);    // empty => all-zero outputs (idle)

        return std::make_unique<hades::LoopbackBackend>(std::move(cfg), seed);
    }

    std::unique_ptr<hades::TransportIF> MakeTransport(
        const std::string&  backend,
        const std::string&  jtagUrl,
        const ManifestInfo& manifest,
        uint64_t            seed,
        hades::DutModel     dutModel)
    {
        if (backend == "loopback")
            return MakeLoopbackTransport(manifest, seed, std::move(dutModel));

        if (backend == "jtag")
        {
            // jtag_url = tcp://host:port (default tcp://127.0.0.1:3122)
            std::string url = jtagUrl.empty() ? "tcp://127.0.0.1:3122" : jtagUrl;

            const std::string scheme = "tcp://";

            if (url.rfind(scheme, 0) != 0)
                throw std::runtime_error("[hades] jtag_url must be tcp://host:port (got '" + url + "')");

            std::string hostport = url.substr(scheme.size());
            std::string host     = hostport.substr(0, hostport.rfind(':'));
            std::string portStr  = hostport.substr(hostport.rfind(':') + 1);

            uint16_t port = static_cast<uint16_t>(std::stoul(portStr));

            return std::make_unique<JtagTcpTransport>(host, port);
        }

        throw std::runtime_error("[hades] backend '" + backend +
            "' not supported by cohestra_hades (loopback|jtag)");
    }


    // ---- JtagTcpTransport -----------------------------------------------------

    namespace {

        constexpr std::uint32_t kWinRegs = 0x1000;  // bridge register window
        constexpr std::uint32_t kWinH2C  = 0x2000;  // H2C frame mailbox
        constexpr std::uint32_t kWinC2H  = 0x3000;  // C2H frame mailbox

        constexpr std::uint32_t kRegStatus = kWinRegs + 0x0C;

        void PutU32(std::vector<std::uint8_t>& out, std::uint32_t v)
        {
            out.push_back(std::uint8_t(v));
            out.push_back(std::uint8_t(v >> 8));
            out.push_back(std::uint8_t(v >> 16));
            out.push_back(std::uint8_t(v >> 24));
        }

        std::uint32_t GetU32(const std::uint8_t* p)
        {
            return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
                   (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
        }
    }

    JtagTcpTransport::JtagTcpTransport(const std::string& host, uint16_t port)
    {
        endpoint = host + ":" + std::to_string(port);

        addrinfo hints{};
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* res = nullptr;

        if (int rc = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res))
            throw std::runtime_error("jtag: getaddrinfo(" + endpoint + "): " + gai_strerror(rc));

        for (addrinfo* ai = res; ai; ai = ai->ai_next)
        {
            sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

            if (sock < 0)
                continue;

            int one = 1;
            setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

            if (connect(sock, ai->ai_addr, ai->ai_addrlen) == 0)
                break;

            close(sock);
            sock = -1;
        }

        freeaddrinfo(res);

        if (sock < 0)
            throw std::runtime_error("jtag: cannot connect to " + endpoint +
                " (is jtag_mmio_server.tcl running?) — " + std::strerror(errno));

        spdlog::info("jtag: connected to {}", endpoint);
    }

    JtagTcpTransport::~JtagTcpTransport()
    {
        if (sock >= 0)
            close(sock);
    }

    void JtagTcpTransport::LatchError(const std::string& what) noexcept
    {
        if (!fatal)
        {
            fatal     = true;
            fatalWhat = what;
            spdlog::error("jtag: {}", what);
        }
    }

    std::vector<std::uint32_t> JtagTcpTransport::RoundTrip(const std::vector<Op>& ops)
    {
        // request payload: 0x01 BATCH, u32 n_ops, op records
        std::vector<std::uint8_t> body;
        body.push_back(0x01);
        PutU32(body, static_cast<std::uint32_t>(ops.size()));

        for (const Op& op : ops)
        {
            body.push_back(op.isWrite ? 0x01 : 0x02);
            body.push_back(0);              // flags
            body.push_back(0); body.push_back(0);   // reserved u16
            PutU32(body, op.addr);
            PutU32(body, static_cast<std::uint32_t>(op.words.size()));

            if (op.isWrite)
                for (std::uint32_t w : op.words)
                    PutU32(body, w);
        }

        std::vector<std::uint8_t> msg;
        PutU32(msg, static_cast<std::uint32_t>(body.size()));
        msg.insert(msg.end(), body.begin(), body.end());

        size_t sent = 0;

        while (sent < msg.size())
        {
            ssize_t n = send(sock, msg.data() + sent, msg.size() - sent, 0);

            if (n <= 0)
                throw std::runtime_error("jtag: send failed: " + std::string(std::strerror(errno)));

            sent += size_t(n);
        }

        auto readExact = [this] (std::uint8_t* dst, size_t len)
        {
            size_t got = 0;

            while (got < len)
            {
                ssize_t n = recv(sock, dst + got, len - got, 0);

                if (n <= 0)
                    throw std::runtime_error("jtag: recv failed: " + std::string(std::strerror(errno)));

                got += size_t(n);
            }
        };

        std::uint8_t hdr[4];
        readExact(hdr, 4);

        const std::uint32_t replyLen = GetU32(hdr);

        if (replyLen < 4 || replyLen > 262144)
            throw std::runtime_error("jtag: bad reply length " + std::to_string(replyLen));

        std::vector<std::uint8_t> reply(replyLen);
        readExact(reply.data(), replyLen);

        if ((reply[0] & 0x7F) != 0x01 || reply[1] != 0)
            throw std::runtime_error("jtag: BATCH reply status != 0");

        const unsigned nOps = reply[2] | (unsigned(reply[3]) << 8);

        std::vector<std::uint32_t> reads;
        size_t off = 4;

        for (unsigned i = 0; i < nOps && off + 4 <= reply.size(); ++i)
        {
            const bool isWrite = reply[off] == 0x01;
            const unsigned st  = reply[off + 1];
            off += 4;

            if (st != 0)
                throw std::runtime_error("jtag: op " + std::to_string(i) + " AXI/txn error");

            if (!isWrite)
            {
                const std::uint32_t count = ops[i].words.size();

                if (off + 4 * size_t(count) > reply.size())
                    throw std::runtime_error("jtag: truncated read reply");

                for (std::uint32_t k = 0; k < count; ++k, off += 4)
                    reads.push_back(GetU32(reply.data() + off));
            }
        }

        return reads;
    }

    std::uint32_t JtagTcpTransport::read_mmio(std::uint64_t offset)
    {
        // never throw across TransportIF (HadesLink drives it from noexcept
        // contexts): latch and report 0 — status().error carries the cause
        try
        {
            Op op { false, static_cast<std::uint32_t>(offset), { 1 } };  // count 1

            auto reads = RoundTrip({ op });

            return reads.empty() ? 0 : reads[0];
        }
        catch (const std::exception& e)
        {
            LatchError(e.what());
            return 0;
        }
    }

    void JtagTcpTransport::write_mmio(std::uint64_t offset, std::uint32_t value)
    {
        try
        {
            RoundTrip({ Op { true, static_cast<std::uint32_t>(offset), { value } } });
        }
        catch (const std::exception& e)
        {
            LatchError(e.what());
        }
    }

    void JtagTcpTransport::push(hades::ChannelId, std::span<const std::byte> frame)
    {
        if (fatal)
            return;

        // Phase J-0: the bridge maps one 64B frame to one AXIS beat; anything
        // larger would be misread by the fabric as multiple frames
        if (frame.size() != hades::kFrameAlign)
        {
            LatchError("Phase J-0 bridge carries single-beat 64B frames only; "
                "TOKEN_BATCH_WIDE payloads (oceanus channels) need the Phase J-1 "
                "multi-beat bridge — use backend=loopback until then");
            return;
        }

        outbox.emplace_back(frame.begin(), frame.end());
    }

    bool JtagTcpTransport::pull(hades::ChannelId chan, std::span<std::byte>& out)
    {
        // head-of-line per channel, same shape as LoopbackBackend::pull
        if (inbox.empty())
            return false;

        const auto& front = inbox.front();

        if (front.size() < hades::kFrameHeaderBytes ||
            static_cast<hades::ChannelId>(front[5]) != chan)    // header byte 5 = chan_id (§3.1)
            return false;

        rxHold = front;     // copy: the span must survive the pop (T4 lesson)
        inbox.pop_front();

        out = std::span<std::byte>(rxHold.data(), rxHold.size());
        return true;
    }

    void JtagTcpTransport::RefreshStatus()
    {
        bridgeStatus = read_mmio(kRegStatus);
        fabricStatus = read_mmio(0x008);    // fabric CSR STATUS (§6)
    }

    void JtagTcpTransport::FlushOutbox()
    {
        const unsigned h2cFree = bridgeStatus & 0x7F;   // STATUS[6:0] free 64B slots

        if (outbox.empty())
            return;

        if (h2cFree == 0)
            return;     // mailbox full: retry next poll (never silently drop, §1.4)

        std::vector<Op> ops;
        size_t n = 0;

        for (auto it = outbox.begin(); it != outbox.end() && n < h2cFree; ++it, ++n)
        {
            Op op { true, kWinH2C, {} };    // 16 x u32 = one 64B frame, LSB-first
            op.words.reserve(hades::kFrameAlign / 4);

            for (size_t w = 0; w < hades::kFrameAlign / 4; ++w)
                op.words.push_back(GetU32(reinterpret_cast<const std::uint8_t*>(it->data()) + 4 * w));

            ops.push_back(std::move(op));
        }

        RoundTrip(ops);
        outbox.erase(outbox.begin(), outbox.begin() + n);
    }

    void JtagTcpTransport::poll()
    {
        if (fatal)
            return;

        try
        {
            RefreshStatus();
            FlushOutbox();

            // drain the c2h mailbox: one 64B read burst per occupancy slot
            const unsigned c2hOcc = (bridgeStatus >> 8) & 0x7F;

            if (c2hOcc)
            {
                std::vector<Op> ops(c2hOcc, Op { false, kWinC2H, std::vector<std::uint32_t>(16, 0) });

                auto reads = RoundTrip(ops);

                for (unsigned f = 0; f < c2hOcc; ++f)
                {
                    std::vector<std::byte> frame(hades::kFrameAlign);

                    for (size_t w = 0; w < 16; ++w)
                    {
                        const std::uint32_t word = reads[f * 16 + w];

                        for (size_t b = 0; b < 4; ++b)
                            frame[4 * w + b] = std::byte(word >> (8 * b));
                    }

                    inbox.push_back(std::move(frame));
                }
            }

            if (bridgeStatus & (0x1Fu << 16))   // sticky ERR bits [20:16]
                LatchError("bridge STATUS error bits set: " + std::to_string(bridgeStatus));
        }
        catch (const std::exception& e)
        {
            LatchError(e.what());
        }
    }

    hades::LinkStatus JtagTcpTransport::status()
    {
        hades::LinkStatus st;

        st.link_up      = !fatal;
        st.ticking      = (fabricStatus >> 1) & 1;
        st.stalled      = (fabricStatus >> 2) & 1;
        st.error        = fatal || ((fabricStatus >> 3) & 1);
        st.stall_reason = 0;

        return st;
    }
}

#endif
