#pragma once

#include "chi_lunatic_config.hpp"
#include "chi_lunatic_events.hpp"
#include "chi_lunatic_state.hpp"
#include "chi_lunatic_types.hpp"
#include "../../../chi_eb/spec/chi_eb_protocol.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace CHI::Lunatic {

    class SparseMemory;

    class Core {
    public:
        struct Impl;

    private:
        std::unique_ptr<Impl>           impl;

    public:
        Core(NodeId nodeId, Options options, std::shared_ptr<EventHub> events);
        ~Core();
        Core(const Core&) = delete;
        Core&                           operator=(const Core&) = delete;

        void                            Reset();
        void                            Tick(Time time);
        bool                            Drain();
        bool                            DispatchEvents();
        bool                            HasInflight() const noexcept;

        bool                            PushRXREQ(const Request& flit);
        bool                            PushRXRSP(const Response& flit);
        bool                            PushRXDAT(const Data& flit);

        std::optional<Request>          PeekTXREQ() const;
        std::optional<Response>         PeekTXRSP() const;
        std::optional<Data>             PeekTXDAT() const;
        std::optional<Snoop>            PeekTXSNP() const;

        std::optional<Request>          PopTXREQ();
        std::optional<Response>         PopTXRSP();
        std::optional<Data>             PopTXDAT();
        std::optional<Snoop>            PopTXSNP();

        NodeId                          GetNodeId() const noexcept;
        Time                            GetTime() const noexcept;
        const Options&                  GetOptions() const noexcept;
        std::shared_ptr<EventHub>       GetEvents() const noexcept;
        const std::vector<ErrorInfo>&   GetErrors() const noexcept;
        SparseMemory&                   GetMemory() noexcept;
        const SparseMemory&             GetMemory() const noexcept;
        HomeLineInfo                    InspectHome(const LineKey& line) const noexcept;
        CacheState                      InspectRequester(const LineKey& line, NodeId requester) const noexcept;
        bool                            ValidateInvariants() const noexcept;
    };

    template<CHI::Eb::FlitConfigurationConcept config>
    class HomeNode {
    public:
        using REQFlit = CHI::Eb::Flits::REQ<config>;
        using RSPFlit = CHI::Eb::Flits::RSP<config>;
        using DATFlit = CHI::Eb::Flits::DAT<config>;
        using SNPFlit = CHI::Eb::Flits::SNP<config>;

        struct SNPEmission {
            NodeId                  target = 0;
            SNPFlit                 flit{};
        };

    private:
        Core                        core;

        static Request              Decode(const REQFlit& flit)
        {
            Request                 result;
            result.source = flit.SrcID();
            result.target = flit.TgtID();
            result.txnId = flit.TxnID();
            result.address = flit.Addr();
            result.opcode = flit.Opcode();
            result.returnNid = flit.ReturnNID();
            result.returnTxnId = flit.ReturnTxnID();
            result.size = flit.Size();
            result.qos = flit.QoS();
            result.order = flit.Order();
            result.memAttr = flit.MemAttr();
            result.snpAttr = flit.SnpAttr();
            result.pCrdType = flit.PCrdType();
            result.traceTag = flit.TraceTag();
            result.nonSecure = flit.NS();
            result.allowRetry = flit.AllowRetry();
            result.expCompAck = flit.ExpCompAck();
            result.likelyShared = flit.LikelyShared();
            return result;
        }

        static Response             Decode(const RSPFlit& flit)
        {
            Response                result;
            result.source = flit.SrcID();
            result.target = flit.TgtID();
            result.txnId = flit.TxnID();
            result.dbid = flit.DBID();
            result.opcode = flit.Opcode();
            result.resp = flit.Resp();
            result.respErr = flit.RespErr();
            result.fwdState = flit.FwdState();
            result.pCrdType = flit.PCrdType();
            result.qos = flit.QoS();
            result.cBusy = flit.CBusy();
            result.traceTag = flit.TraceTag();
            return result;
        }

        static Data                 Decode(const DATFlit& flit)
        {
            Data                    result;
            result.source = flit.SrcID();
            result.target = flit.TgtID();
            result.homeNid = flit.HomeNID();
            result.txnId = flit.TxnID();
            result.dbid = flit.DBID();
            result.opcode = flit.Opcode();
            result.resp = flit.Resp();
            result.respErr = flit.RespErr();
            result.dataSource = flit.DataSource();
            result.dataId = flit.DataID();
            result.ccid = flit.CCID();
            result.qos = flit.QoS();
            result.cBusy = flit.CBusy();
            result.traceTag = flit.TraceTag();

            constexpr size_t        lanes = config::dataWidth / 64;
            constexpr size_t        bytes = config::dataWidth / 8;
            for (size_t lane = 0; lane < lanes; ++lane)
            {
                const uint64_t      word = flit.Data()[lane];
                for (size_t byte = 0; byte < 8; ++byte)
                    result.bytes[lane * 8 + byte] = uint8_t(word >> (byte * 8));
            }

            const uint64_t          enables = flit.BE();
            for (size_t byte = 0; byte < bytes; ++byte)
                result.byteEnables[byte] = uint8_t((enables >> byte) & 1);

            if constexpr (DATFlit::hasPoison)
            {
                const uint64_t      poison = flit.Poison();
                for (size_t lane = 0; lane < lanes; ++lane)
                    result.poison[lane] = uint8_t((poison >> lane) & 1);
            }
            if constexpr (DATFlit::hasDataCheck)
            {
                const uint64_t      check = flit.DataCheck();
                for (size_t byte = 0; byte < bytes; ++byte)
                    result.dataCheck[byte] = uint8_t((check >> byte) & 1);
            }
            return result;
        }

        static REQFlit              Encode(const Request& value)
        {
            REQFlit                 flit{};
            flit.QoS() = value.qos;
            flit.TgtID() = value.target;
            flit.SrcID() = value.source;
            flit.TxnID() = value.txnId;
            flit.ReturnNID() = value.returnNid;
            flit.ReturnTxnID() = value.returnTxnId;
            flit.Opcode() = value.opcode;
            flit.Size() = value.size;
            flit.Addr() = value.address;
            flit.NS() = value.nonSecure;
            flit.LikelyShared() = value.likelyShared;
            flit.AllowRetry() = value.allowRetry;
            flit.Order() = value.order;
            flit.PCrdType() = value.pCrdType;
            flit.MemAttr() = value.memAttr;
            flit.SnpAttr() = value.snpAttr;
            flit.ExpCompAck() = value.expCompAck;
            flit.TraceTag() = value.traceTag;
            return flit;
        }

        static RSPFlit              Encode(const Response& value)
        {
            RSPFlit                 flit{};
            flit.QoS() = value.qos;
            flit.TgtID() = value.target;
            flit.SrcID() = value.source;
            flit.TxnID() = value.txnId;
            flit.Opcode() = value.opcode;
            flit.RespErr() = value.respErr;
            flit.Resp() = value.resp;
            flit.FwdState() = value.fwdState;
            flit.CBusy() = value.cBusy;
            flit.DBID() = value.dbid;
            flit.PCrdType() = value.pCrdType;
            flit.TraceTag() = value.traceTag;
            return flit;
        }

        static DATFlit              Encode(const Data& value)
        {
            DATFlit                 flit{};
            flit.QoS() = value.qos;
            flit.TgtID() = value.target;
            flit.SrcID() = value.source;
            flit.TxnID() = value.txnId;
            flit.HomeNID() = value.homeNid;
            flit.Opcode() = value.opcode;
            flit.RespErr() = value.respErr;
            flit.Resp() = value.resp;
            flit.DataSource() = value.dataSource;
            flit.CBusy() = value.cBusy;
            flit.DBID() = value.dbid;
            flit.CCID() = value.ccid;
            flit.DataID() = value.dataId;
            flit.TraceTag() = value.traceTag;

            constexpr size_t        lanes = config::dataWidth / 64;
            constexpr size_t        bytes = config::dataWidth / 8;
            for (size_t lane = 0; lane < lanes; ++lane)
            {
                uint64_t            word = 0;
                for (size_t byte = 0; byte < 8; ++byte)
                    word |= uint64_t(value.bytes[lane * 8 + byte]) << (byte * 8);
                flit.Data()[lane] = word;
            }

            uint64_t                enables = 0;
            for (size_t byte = 0; byte < bytes; ++byte)
                enables |= uint64_t(value.byteEnables[byte] & 1) << byte;
            flit.BE() = enables;

            if constexpr (DATFlit::hasPoison)
            {
                uint64_t            poison = 0;
                for (size_t lane = 0; lane < lanes; ++lane)
                    poison |= uint64_t(value.poison[lane] & 1) << lane;
                flit.Poison() = poison;
            }
            if constexpr (DATFlit::hasDataCheck)
            {
                uint64_t            check = 0;
                for (size_t byte = 0; byte < bytes; ++byte)
                    check |= uint64_t(value.dataCheck[byte] & 1) << byte;
                flit.DataCheck() = check;
            }
            return flit;
        }

        static SNPEmission          Encode(const Snoop& value)
        {
            SNPEmission             emission;
            emission.target = value.target;
            emission.flit.QoS() = value.qos;
            emission.flit.SrcID() = value.source;
            emission.flit.TxnID() = value.txnId;
            emission.flit.Opcode() = value.opcode;
            emission.flit.Addr() = value.address >> 3;
            emission.flit.NS() = value.nonSecure;
            emission.flit.DoNotGoToSD() = value.doNotGoToSD;
            emission.flit.RetToSrc() = value.retToSrc;
            emission.flit.TraceTag() = value.traceTag;
            return emission;
        }

    public:
        explicit HomeNode(NodeId nodeId, Options options = {}, std::shared_ptr<EventHub> events = nullptr)
            : core(nodeId, [&options] {
                if (options.dataWidth != config::dataWidth)
                    throw std::invalid_argument("Lunatic options and flit data widths differ");
                return options;
            }(), std::move(events))
        {
        }

        void                        Reset() { core.Reset(); }
        void                        Tick(Time time) { core.Tick(time); }
        bool                        Drain() { return core.Drain(); }
        bool                        DispatchEvents() { return core.DispatchEvents(); }
        bool                        HasInflight() const noexcept { return core.HasInflight(); }

        bool                        PushRXREQ(const REQFlit& flit) { return core.PushRXREQ(Decode(flit)); }
        bool                        PushRXRSP(const RSPFlit& flit) { return core.PushRXRSP(Decode(flit)); }
        bool                        PushRXDAT(const DATFlit& flit) { return core.PushRXDAT(Decode(flit)); }

        std::optional<REQFlit>      PeekTXREQ() const
        {
            const auto              value = core.PeekTXREQ();
            return value ? std::optional<REQFlit>(Encode(*value)) : std::nullopt;
        }

        std::optional<RSPFlit>      PeekTXRSP() const
        {
            const auto              value = core.PeekTXRSP();
            return value ? std::optional<RSPFlit>(Encode(*value)) : std::nullopt;
        }

        std::optional<DATFlit>      PeekTXDAT() const
        {
            const auto              value = core.PeekTXDAT();
            return value ? std::optional<DATFlit>(Encode(*value)) : std::nullopt;
        }

        std::optional<SNPEmission>  PeekTXSNP() const
        {
            const auto              value = core.PeekTXSNP();
            return value ? std::optional<SNPEmission>(Encode(*value)) : std::nullopt;
        }

        std::optional<REQFlit>      PopTXREQ()
        {
            const auto              value = core.PopTXREQ();
            return value ? std::optional<REQFlit>(Encode(*value)) : std::nullopt;
        }

        std::optional<RSPFlit>      PopTXRSP()
        {
            const auto              value = core.PopTXRSP();
            return value ? std::optional<RSPFlit>(Encode(*value)) : std::nullopt;
        }

        std::optional<DATFlit>      PopTXDAT()
        {
            const auto              value = core.PopTXDAT();
            return value ? std::optional<DATFlit>(Encode(*value)) : std::nullopt;
        }

        std::optional<SNPEmission>  PopTXSNP()
        {
            const auto              value = core.PopTXSNP();
            return value ? std::optional<SNPEmission>(Encode(*value)) : std::nullopt;
        }

        NodeId                      GetNodeId() const noexcept { return core.GetNodeId(); }
        Time                        GetTime() const noexcept { return core.GetTime(); }
        const Options&              GetOptions() const noexcept { return core.GetOptions(); }
        std::shared_ptr<EventHub>   GetEvents() const noexcept { return core.GetEvents(); }
        const std::vector<ErrorInfo>&
                                    GetErrors() const noexcept { return core.GetErrors(); }
        SparseMemory&               GetMemory() noexcept { return core.GetMemory(); }
        const SparseMemory&         GetMemory() const noexcept { return core.GetMemory(); }
        HomeLineInfo                InspectHome(const LineKey& line) const noexcept { return core.InspectHome(line); }
        CacheState                  InspectRequester(const LineKey& line, NodeId requester) const noexcept
        {
            return core.InspectRequester(line, requester);
        }
        bool                        ValidateInvariants() const noexcept { return core.ValidateInvariants(); }
    };

}
