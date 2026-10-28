#pragma once

#include "driver.hpp"
#include "oracle.hpp"

#include <memory>

namespace LunaticTest {

template<typename config>
class CheckedDriver {
public:
    using Base = Driver<config>;
    using REQ = typename Base::REQ;
    using RSP = typename Base::RSP;
    using DAT = typename Base::DAT;
    using SNP = typename Base::SNP;

    Base driver;
    CHI::Lunatic::NodeId home;
    std::map<CHI::Lunatic::NodeId, std::unique_ptr<RNOracle<config>>> oracles;

    explicit CheckedDriver(CHI::Lunatic::Options options = {}, CHI::Lunatic::NodeId home = 2)
        : driver(options, home), home(home)
    {
    }

    RNOracle<config>& Oracle(CHI::Lunatic::NodeId requester)
    {
        auto& oracle = oracles[requester];
        if (!oracle)
            oracle = std::make_unique<RNOracle<config>>(home, requester);
        return *oracle;
    }

    REQ Request(uint16_t opcode, CHI::Lunatic::NodeId source, CHI::Lunatic::TransactionId txnId,
                uint64_t address, bool expCompAck = false)
    {
        return driver.Request(opcode, source, txnId, address, expCompAck);
    }

    void PushRequest(const REQ& request)
    {
        try {
            Oracle(request.SrcID()).Request(driver.now, request);
        } catch (const std::exception& error) {
            throw std::runtime_error("request opcode=" + std::to_string(uint16_t(request.Opcode())) +
                " txn=" + std::to_string(uint16_t(request.TxnID())) + ": " + error.what());
        }
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
    }

    RSP NextRSP(uint16_t opcode)
    {
        RSP response = driver.NextRSP(opcode);
        Oracle(response.TgtID()).Response(driver.now, response);
        return response;
    }

    DAT NextDAT(uint16_t opcode)
    {
        DAT data = driver.NextDAT(opcode);
        Oracle(data.TgtID()).Data(driver.now, data);
        return data;
    }

    SNP NextSNP(uint16_t opcode)
    {
        SNP snoop = driver.NextSNP(opcode);
        Oracle(snoop.target).Snoop(driver.now, snoop.flit);
        return snoop;
    }

    void CompAck(CHI::Lunatic::NodeId source, CHI::Lunatic::DatabaseId dbid)
    {
        Oracle(source).CompAck(driver.now, dbid);
        driver.CompAck(source, dbid);
    }

    void WriteData(CHI::Lunatic::NodeId source, CHI::Lunatic::TransactionId txnId,
                   CHI::Lunatic::DatabaseId dbid, const CHI::Lunatic::LineData& line,
                   const CHI::Lunatic::ByteEnables& enables, uint16_t opcode, uint8_t resp = 0)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < CHI::Lunatic::LINE_SIZE / bytes; ++beat)
        {
            DAT data{};
            data.BE() = 0;
            data.SrcID() = source;
            data.TgtID() = home;
            data.HomeNID() = 0;
            data.TxnID() = opcode == CHI::Eb::Opcodes::DAT::CopyBackWrData ? dbid : txnId;
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
            Oracle(source).WriteData(driver.now, data);
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
    }

    void SnoopResponse(CHI::Lunatic::NodeId source, CHI::Lunatic::TransactionId txnId, uint8_t resp)
    {
        RSP response{};
        response.SrcID() = source;
        response.TgtID() = home;
        response.TxnID() = txnId;
        response.Opcode() = CHI::Eb::Opcodes::RSP::SnpResp;
        response.Resp() = resp;
        Oracle(source).SnoopResponse(driver.now, response);
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    void SnoopData(CHI::Lunatic::NodeId source, CHI::Lunatic::TransactionId txnId,
                   const CHI::Lunatic::LineData& line, uint8_t resp, bool partial = false)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < CHI::Lunatic::LINE_SIZE / bytes; ++beat)
        {
            DAT data{};
            data.BE() = 0;
            data.SrcID() = source;
            data.TgtID() = home;
            data.HomeNID() = 0;
            data.TxnID() = txnId;
            data.Opcode() = partial ? CHI::Eb::Opcodes::DAT::SnpRespDataPtl : CHI::Eb::Opcodes::DAT::SnpRespData;
            data.Resp() = resp;
            data.DataID() = beat * bytes / 16;
            data.CCID() = (beat * bytes / 16) % 2;
            for (size_t index = 0; index < bytes; ++index)
            {
                data.Data()[index / 8] |= uint64_t(line[beat * bytes + index]) << ((index % 8) * 8);
                if (!partial || index < bytes / 2)
                    data.BE() = uint64_t(data.BE()) | (uint64_t(1) << index);
            }
            Oracle(source).SnoopData(driver.now, data);
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
    }

    CHI::Lunatic::LineData ReadData(CHI::Lunatic::NodeId source, CHI::Lunatic::TransactionId txnId,
                                    uint64_t address, uint16_t opcode, bool compAck = true)
    {
        PushRequest(Request(opcode, source, txnId, address, compAck));
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

    void Quiesce()
    {
        driver.Quiesce();
        for (auto& [requester, oracle] : oracles)
            LUNATIC_CHECK(!oracle->HasInflight());
        LUNATIC_CHECK(driver.node.ValidateInvariants());
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }
};

}
