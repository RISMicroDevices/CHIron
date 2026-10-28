#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

LUNATIC_TEST(Stage4LegalUDPReadAndCMO)
{
    for (PolicyKind policy : { PolicyKind::NINE, PolicyKind::Exclusive, PolicyKind::Inclusive })
    {
        Options options;
        options.policy = policy;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x28000, false);
        LineData data{};
        ByteEnables enables{};
        for (size_t index = 0; index < data.size(); ++index)
        {
            data[index] = uint8_t(41 + index * 3);
            if (index % 32 < 16)
                enables[index] = 1;
        }

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackPtl, 4, 1, line.address));
        auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(4, 1, dbid.DBID(), data, enables, CHI::Eb::Opcodes::DAT::CopyBackWrData,
                         CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LineData expected = data;
        for (size_t index = 0; index < expected.size(); ++index)
            if (index % 32 >= 16)
                expected[index] = 0;

        LUNATIC_CHECK(driver.ReadData(5, 2, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == expected);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::UDP);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 3, line.address, true));
        auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
        driver.SnoopData(5, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespDataPtl_I_PD, true);
        DatabaseId responseDBID = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            auto response = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(response.Resp() == CHI::Eb::Resps::CompData_UD_PD);
            responseDBID = response.DBID();
        }
        driver.CompAck(6, responseDBID);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UDP);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanInvalid, 7, 4, line.address));
        auto clean = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
        driver.SnoopData(6, clean.flit.TxnID(), data, CHI::Eb::Resps::SnpRespDataPtl_I_PD, true);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == expected);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
    }
}
