#pragma once

#include "common.hpp"

namespace LunaticTest {

template<typename config>
class Driver {
public:
    using Node = CHI::Lunatic::HomeNode<config>;
    using REQ = typename Node::REQFlit;
    using RSP = typename Node::RSPFlit;
    using DAT = typename Node::DATFlit;
    using SNP = typename Node::SNPEmission;

    Node node;
    uint64_t now = 0;

    explicit Driver(CHI::Lunatic::Options options = {}, CHI::Lunatic::NodeId home = 2)
        : node(home, [&options] {
            options.dataWidth = config::dataWidth;
            return options;
        }())
    {
    }

    void Tick() { node.Tick(++now); }

    template<typename Predicate>
    void Until(Predicate&& predicate, size_t limit = 500)
    {
        for (size_t cycle = 0; cycle < limit; ++cycle)
        {
            if (predicate())
                return;
            Tick();
        }
        throw std::runtime_error("Lunatic driver condition timed out");
    }

    void Quiesce(size_t limit = 500)
    {
        for (size_t cycle = 0; cycle < limit; ++cycle)
        {
            if (node.Drain())
                return;
            Tick();
        }
        throw std::runtime_error("Lunatic driver failed to drain");
    }

    REQ Request(uint16_t opcode, CHI::Lunatic::NodeId source, CHI::Lunatic::TransactionId txnId,
                uint64_t address, bool expCompAck = false) const
    {
        REQ request{};
        request.SrcID() = source;
        request.TgtID() = node.GetNodeId();
        request.TxnID() = txnId;
        request.Opcode() = opcode;
        request.Addr() = address;
        request.Size() = 6;
        request.AllowRetry() = 1;
        request.ExpCompAck() = expCompAck;
        request.SnpAttr() = 1;
        request.MemAttr() = CHI::Lunatic::IsCoherentRequest(opcode) || opcode == CHI::Eb::Opcodes::REQ::ReadOnce
            ? CHI::Eb::MemAttrs::WriteBack_Allocate : CHI::Eb::MemAttrs::NonCacheable_Bufferable;
        return request;
    }

    RSP NextRSP(uint16_t opcode)
    {
        Until([&] { return static_cast<bool>(node.PeekTXRSP()); });
        auto flit = node.PopTXRSP();
        LUNATIC_CHECK(flit);
        LUNATIC_CHECK(flit->Opcode() == opcode);
        return *flit;
    }

    DAT NextDAT(uint16_t opcode)
    {
        Until([&] { return static_cast<bool>(node.PeekTXDAT()); });
        auto flit = node.PopTXDAT();
        LUNATIC_CHECK(flit);
        LUNATIC_CHECK(flit->Opcode() == opcode);
        return *flit;
    }

    REQ NextREQ(uint16_t opcode)
    {
        Until([&] { return static_cast<bool>(node.PeekTXREQ()); });
        auto flit = node.PopTXREQ();
        LUNATIC_CHECK(flit);
        LUNATIC_CHECK(flit->Opcode() == opcode);
        return *flit;
    }

    SNP NextSNP(uint16_t opcode)
    {
        Until([&] { return static_cast<bool>(node.PeekTXSNP()); });
        auto flit = node.PopTXSNP();
        LUNATIC_CHECK(flit);
        LUNATIC_CHECK(flit->flit.Opcode() == opcode);
        return *flit;
    }

    void CompAck(CHI::Lunatic::NodeId source, CHI::Lunatic::DatabaseId dbid)
    {
        RSP response{};
        response.SrcID() = source;
        response.TgtID() = node.GetNodeId();
        response.TxnID() = dbid;
        response.Opcode() = CHI::Eb::Opcodes::RSP::CompAck;
        LUNATIC_CHECK(node.PushRXRSP(response));
        Tick();
    }

    void WriteData(CHI::Lunatic::NodeId source, CHI::Lunatic::TransactionId txnId,
                   CHI::Lunatic::DatabaseId dbid, const CHI::Lunatic::LineData& line,
                   const CHI::Lunatic::ByteEnables& enables, uint16_t opcode, uint8_t resp = 0)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < CHI::Lunatic::LINE_SIZE / bytes; ++beat)
        {
            DAT data{};
            data.SrcID() = source;
            data.TgtID() = node.GetNodeId();
            data.HomeNID() = node.GetNodeId();
            data.TxnID() = txnId;
            data.DBID() = dbid;
            data.Opcode() = opcode;
            data.Resp() = resp;
            data.DataID() = beat * bytes / 16;
            data.CCID() = (beat * bytes / 16) % 2;
            for (size_t index = 0; index < bytes; ++index)
            {
                data.Data()[index / 8] |= uint64_t(line[beat * bytes + index]) << ((index % 8) * 8);
                if (enables[beat * bytes + index])
                    data.BE() = uint64_t(data.BE()) | (uint64_t(1) << index);
            }
            LUNATIC_CHECK(node.PushRXDAT(data));
        }
        Tick();
    }

    CHI::Lunatic::LineData ReadData(CHI::Lunatic::NodeId source, CHI::Lunatic::TransactionId txnId,
                                    uint64_t address, uint16_t opcode, bool compAck = false)
    {
        LUNATIC_CHECK(node.PushRXREQ(Request(opcode, source, txnId, address, compAck)));
        CHI::Lunatic::LineData result{};
        constexpr size_t bytes = config::dataWidth / 8;
        CHI::Lunatic::DatabaseId dbid = 0;
        for (size_t beat = 0; beat < CHI::Lunatic::LINE_SIZE / bytes; ++beat)
        {
            DAT data = NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TgtID() == source);
            LUNATIC_CHECK(data.TxnID() == txnId);
            LUNATIC_CHECK(data.DataID() == beat * bytes / 16);
            dbid = data.DBID();
            for (size_t index = 0; index < bytes; ++index)
                result[beat * bytes + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
        }
        if (compAck)
            CompAck(source, dbid);
        return result;
    }
};

}
