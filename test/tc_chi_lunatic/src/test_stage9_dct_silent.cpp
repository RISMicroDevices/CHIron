#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData SilentData()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(19 + index * 7);
        return data;
    }

    void CleanUnique(Driver<Config256>& driver, const LineKey& line)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::CleanUnique, 6, 90, line.address, true)));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.CompAck(6, response.DBID());
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.InspectRequester(line, 6) == CacheState::UCE);
    }

    void CheckDirectData(const Driver<Config256>::SNP& snoop, const LineKey& line, const LineData& expected)
    {
        constexpr size_t bytes = Config256::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            Driver<Config256>::DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 4;
            data.HomeNID() = 2;
            data.TxnID() = 1;
            data.DBID() = snoop.flit.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = CHI::Eb::Resps::CompData_SC;
            data.DataID() = beat * bytes / 16;
            data.CCID() = (line.address >> 4) & 3;
            data.BE() = ~uint64_t(0);
            for (size_t index = 0; index < bytes; ++index)
            {
                data.Data()[index / 8] |= uint64_t(expected[beat * bytes + index]) << ((index % 8) * 8);
                LUNATIC_CHECK(uint8_t(data.Data()[index / 8] >> ((index % 8) * 8)) == expected[beat * bytes + index]);
            }
            LUNATIC_CHECK(data.DBID() == snoop.flit.TxnID());
            LUNATIC_CHECK(data.HomeNID() == 2);
        }
    }

    void ForwardedResponse(Driver<Config256>& driver, TransactionId txnId, uint8_t resp, uint8_t fwdState)
    {
        Driver<Config256>::RSP response{};
        response.SrcID() = 6;
        response.TgtID() = driver.node.GetNodeId();
        response.TxnID() = txnId;
        response.Opcode() = CHI::Eb::Opcodes::RSP::SnpRespFwded;
        response.Resp() = resp;
        response.FwdState() = fwdState;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

}

LUNATIC_TEST(Stage9DCTSilentUCEToUDForwarding)
{
    Options options;
    options.dctEnabled = true;
    Driver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x5f000, false);
    const auto expected = SilentData();
    CleanUnique(driver, line);

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, line.address, true);
    request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpSharedFwd);
    LUNATIC_CHECK(snoop.target == 6);
    CheckDirectData(snoop, line, expected);

    ForwardedResponse(driver, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_SD, CHI::Eb::FwdStates::SC);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    driver.CompAck(4, snoop.flit.TxnID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.InspectRequester(line, 4) == CacheState::SC);
    LUNATIC_CHECK(driver.node.InspectRequester(line, 6) == CacheState::SD);
}
