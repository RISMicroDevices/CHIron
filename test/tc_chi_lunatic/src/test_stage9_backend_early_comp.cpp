#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    enum class CompPoint {
        BeforeDBID,
        BeforeData,
        DuringData,
        AfterData
    };

    void SendResponse(Driver<Config256>& driver, const Driver<Config256>::REQ& request, uint16_t opcode)
    {
        Driver<Config256>::RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = request.TxnID();
        response.DBID() = 77;
        response.Opcode() = opcode;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    void SendUpstreamData(Driver<Config256>& driver, DataBufferId dbid, const LineData& expected)
    {
        constexpr size_t bytes = Config256::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            Driver<Config256>::DAT data{};
            data.SrcID() = 4;
            data.TgtID() = 2;
            data.TxnID() = dbid;
            data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
            data.DataID() = beat * bytes / 16;
            data.CCID() = 0;
            for (size_t index = 0; index < bytes; ++index)
            {
                data.Data()[index / 8] |= uint64_t(expected[beat * bytes + index]) << ((index % 8) * 8);
                data.BE() |= uint64_t(1) << index;
            }
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
    }

    void CheckWriteBeat(const Driver<Config256>::DAT& data, size_t beat, const LineData& expected)
    {
        constexpr size_t bytes = Config256::dataWidth / 8;
        LUNATIC_CHECK(data.TgtID() == 6);
        LUNATIC_CHECK(data.TxnID() == 77);
        LUNATIC_CHECK(data.DataID() == beat * bytes / 16);
        for (size_t index = 0; index < bytes; ++index)
            LUNATIC_CHECK(uint8_t(data.Data()[index / 8] >> ((index % 8) * 8)) == expected[beat * bytes + index]);
    }

    void RunEarlyComp(CompPoint point)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.outputQueueLimit = 1;
        Driver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x63000, false);
        LineData expected{};
        for (size_t index = 0; index < expected.size(); ++index)
            expected[index] = uint8_t(29 + index * 7);

        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 1, line.address)));
        const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        SendUpstreamData(driver, dbid, expected);
        const auto request = driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
        if (point == CompPoint::BeforeDBID)
        {
            SendResponse(driver, request, CHI::Eb::Opcodes::RSP::Comp);
            LUNATIC_CHECK(driver.node.GetErrors().empty());
        }
        SendResponse(driver, request, CHI::Eb::Opcodes::RSP::DBIDResp);
        if (point == CompPoint::BeforeData)
        {
            SendResponse(driver, request, CHI::Eb::Opcodes::RSP::Comp);
            LUNATIC_CHECK(driver.node.GetErrors().empty());
        }
        if (point == CompPoint::DuringData || point == CompPoint::AfterData)
            CheckWriteBeat(driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData), 0, expected);
        if (point == CompPoint::DuringData)
        {
            SendResponse(driver, request, CHI::Eb::Opcodes::RSP::Comp);
            LUNATIC_CHECK(driver.node.GetErrors().empty());
        }
        if (point == CompPoint::BeforeDBID || point == CompPoint::BeforeData)
            CheckWriteBeat(driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData), 0, expected);
        CheckWriteBeat(driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData), 1, expected);
        if (point == CompPoint::AfterData)
            SendResponse(driver, request, CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 1);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9CHIBackendAcceptsCompBeforeDBID)
{
    RunEarlyComp(CompPoint::BeforeDBID);
}

LUNATIC_TEST(Stage9CHIBackendAcceptsCompBeforeData)
{
    RunEarlyComp(CompPoint::BeforeData);
}

LUNATIC_TEST(Stage9CHIBackendAcceptsCompDuringData)
{
    RunEarlyComp(CompPoint::DuringData);
}

LUNATIC_TEST(Stage9CHIBackendAcceptsCompAfterData)
{
    RunEarlyComp(CompPoint::AfterData);
}
