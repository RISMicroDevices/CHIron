#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = CheckedDriver<Config256>;
    using RSP = TestDriver::RSP;

    LineData IndependentPattern()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(0x74 + index * 13);
        return data;
    }

    ByteEnables IndependentEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    template<typename Driver>
    auto IndependentRequest(Driver& driver, uint16_t opcode, TransactionId txnId, uint64_t address)
    {
        auto request = driver.Request(opcode, 4, txnId, address);
        request.ReturnNID() = 0;
        request.StashNIDValid() = 0;
        request.StashLPIDValid() = 0;
        request.StashLPID() = 0;
        if (IsSeparatedStashRequest(opcode))
            request.StashGroupID() = 0x25;
        return request;
    }

    RSP PopIndependentResponse(TestDriver& driver)
    {
        driver.driver.Until([&] { return static_cast<bool>(driver.driver.node.PeekTXRSP()); });
        const auto response = driver.driver.node.PopTXRSP();
        LUNATIC_CHECK(response);
        driver.Oracle(response->TgtID()).Response(driver.driver.now, *response);
        return *response;
    }

    void CheckIndependentResponse(TestDriver& driver, uint16_t opcode)
    {
        if (!IsSeparatedStashRequest(opcode))
        {
            const auto comp = PopIndependentResponse(driver);
            LUNATIC_CHECK(comp.Opcode() == CHI::Eb::Opcodes::RSP::Comp);
            LUNATIC_CHECK(comp.Resp() == CHI::Eb::Resps::Comp_SC);
            return;
        }

        const auto first = PopIndependentResponse(driver);
        if (first.Opcode() == CHI::Eb::Opcodes::RSP::CompStashDone)
        {
            LUNATIC_CHECK(first.Resp() == CHI::Eb::Resps::Comp_SC);
            return;
        }
        if (first.Opcode() == CHI::Eb::Opcodes::RSP::StashDone)
        {
            const auto comp = PopIndependentResponse(driver);
            LUNATIC_CHECK(comp.Opcode() == CHI::Eb::Opcodes::RSP::Comp);
            LUNATIC_CHECK(comp.Resp() == CHI::Eb::Resps::Comp_SC);
            return;
        }
        LUNATIC_CHECK(first.Opcode() == CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(first.Resp() == CHI::Eb::Resps::Comp_SC);
        LUNATIC_CHECK(PopIndependentResponse(driver).Opcode() == CHI::Eb::Opcodes::RSP::StashDone);
    }

    void SeedUniquePeer(TestDriver& driver, const LineKey& line, const LineData& data)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, line.address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 90, response.DBID(), data, IndependentEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
            CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(6, 91, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UD);
    }

    void SeedSharedPeer(TestDriver& driver, const LineKey& line, const LineData& data)
    {
        driver.driver.node.GetMemory().WriteFull(line, data);
        LUNATIC_CHECK(driver.ReadData(6, 90, line.address, CHI::Eb::Opcodes::REQ::ReadShared) == data);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::SC);
    }

    void CheckNoPeer(uint16_t opcode, bool homeHit)
    {
        TestDriver driver;
        const LineKey line = LineKey::FromAddress(0x7c000, false);
        const LineData expected = IndependentPattern();
        if (homeHit)
        {
            driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, line.address));
            const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
            driver.WriteData(6, 90, response.DBID(), expected, IndependentEnables(),
                CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
            driver.Quiesce();
            LUNATIC_CHECK(driver.driver.node.InspectHome(line).valid);
        }
        else
            driver.driver.node.GetMemory().WriteFull(line, expected);

        driver.PushRequest(IndependentRequest(driver, opcode, 10, line.address));
        CheckIndependentResponse(driver, opcode);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.driver.node.PeekTXSNP());
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).valid);
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == expected);
        LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
    }

    void CheckUniquePeer(uint16_t opcode)
    {
        TestDriver driver;
        const LineKey line = LineKey::FromAddress(0x7c040, false);
        const LineData expected = IndependentPattern();
        SeedUniquePeer(driver, line, expected);
        driver.PushRequest(IndependentRequest(driver, opcode, 10, line.address));
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
        LUNATIC_CHECK(snoop.target == 6);
        driver.SnoopData(6, snoop.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_I_PD);
        CheckIndependentResponse(driver, opcode);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).valid);
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == expected);
        LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
    }

    void CheckSharedPeer(uint16_t opcode)
    {
        TestDriver driver;
        const LineKey line = LineKey::FromAddress(0x7c080, false);
        const LineData expected = IndependentPattern();
        SeedSharedPeer(driver, line, expected);
        driver.PushRequest(IndependentRequest(driver, opcode, 10, line.address));
        CheckIndependentResponse(driver, opcode);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.driver.node.PeekTXSNP());
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::SC);
        LUNATIC_CHECK(!driver.driver.node.InspectHome(line).valid);
        LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == expected);
        LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9TargetlessIndependentNoPeerFetch)
{
    for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::StashOnceUnique,
                                   CHI::Eb::Opcodes::REQ::StashOnceShared,
                                   CHI::Eb::Opcodes::REQ::StashOnceSepUnique,
                                   CHI::Eb::Opcodes::REQ::StashOnceSepShared })
        CheckNoPeer(opcode, false);
}

LUNATIC_TEST(Stage9TargetlessIndependentHomeHit)
{
    for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::StashOnceUnique,
                                   CHI::Eb::Opcodes::REQ::StashOnceShared,
                                   CHI::Eb::Opcodes::REQ::StashOnceSepUnique,
                                   CHI::Eb::Opcodes::REQ::StashOnceSepShared })
        CheckNoPeer(opcode, true);
}

LUNATIC_TEST(Stage9TargetlessIndependentUniquePeerInvalidated)
{
    for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::StashOnceUnique,
                                   CHI::Eb::Opcodes::REQ::StashOnceShared,
                                   CHI::Eb::Opcodes::REQ::StashOnceSepUnique,
                                   CHI::Eb::Opcodes::REQ::StashOnceSepShared })
        CheckUniquePeer(opcode);
}

LUNATIC_TEST(Stage9TargetlessIndependentSharedPeerPreserved)
{
    for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::StashOnceUnique,
                                   CHI::Eb::Opcodes::REQ::StashOnceShared,
                                   CHI::Eb::Opcodes::REQ::StashOnceSepUnique,
                                   CHI::Eb::Opcodes::REQ::StashOnceSepShared })
        CheckSharedPeer(opcode);
}
