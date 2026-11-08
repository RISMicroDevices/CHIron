#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = CheckedDriver<Config256>;

    LineData TargetlessPattern()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(0x2d + index * 7);
        return data;
    }

    ByteEnables FullTargetlessEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    ByteEnables PartialTargetlessEnables()
    {
        ByteEnables enables{};
        for (size_t index = 7; index < 23; ++index)
            enables[index] = 1;
        return enables;
    }

    template<typename Driver>
    auto TargetlessRequest(Driver& driver, uint16_t opcode, TransactionId txnId, uint64_t address)
    {
        auto request = driver.Request(opcode, 4, txnId, address);
        request.ReturnNID() = 0;
        request.StashNIDValid() = 0;
        request.StashLPIDValid() = 0;
        request.StashLPID() = 0;
        return request;
    }

    LineData MergedTargetlessData(const LineData& initial, const LineData& update, const ByteEnables& enables)
    {
        LineData merged = initial;
        for (size_t index = 0; index < merged.size(); ++index)
            if (enables[index])
                merged[index] = update[index];
        return merged;
    }

    void SeedUniquePeer(TestDriver& driver, const LineKey& line, const LineData& data)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, line.address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 90, response.DBID(), data, FullTargetlessEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(6, 91, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UD);
    }

    void CheckTargetlessWrite(uint16_t opcode, bool uniquePeer)
    {
        TestDriver driver;
        const LineKey line = LineKey::FromAddress(0x7b000, false);
        const LineData update = TargetlessPattern();
        const auto enables = opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFullStash
            ? FullTargetlessEnables() : PartialTargetlessEnables();
        LineData old{};
        if (uniquePeer)
        {
            for (size_t index = 0; index < old.size(); ++index)
                old[index] = uint8_t(0xa3 + index * 3);
            SeedUniquePeer(driver, line, old);
        }

        driver.PushRequest(TargetlessRequest(driver, opcode, 10, line.address));
        if (uniquePeer)
        {
            const auto expectedOpcode = opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFullStash
                ? CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash : CHI::Eb::Opcodes::SNP::SnpUniqueStash;
            const auto snoop = driver.NextSNP(expectedOpcode);
            LUNATIC_CHECK(snoop.target == 6);
            LUNATIC_CHECK(!snoop.flit.StashLPIDValid());
            if (opcode == CHI::Eb::Opcodes::REQ::WriteUniqueFullStash)
                driver.SnoopResponse(6, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
            else
                driver.SnoopData(6, snoop.flit.TxnID(), old, CHI::Eb::Resps::SnpRespData_I_PD);
        }
        const auto dbid = uniquePeer
            ? driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp)
            : driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
        LUNATIC_CHECK(dbid.RespErr() == CHI::Eb::RespErrs::OK);
        driver.WriteData(4, 10, dbid.DBID(), update, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        if (!uniquePeer)
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::OK);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.driver.node.PeekTXSNP());
        const auto& home = driver.driver.node.InspectHome(line);
        LUNATIC_CHECK(home.valid);
        LUNATIC_CHECK(home.data == MergedTargetlessData(uniquePeer ? old : LineData{}, update, enables));
        LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
    }

    void CheckInvalidTarget(uint16_t opcode, bool validNode, bool validLpid)
    {
        Driver<Config256> driver;
        const LineKey line = LineKey::FromAddress(0x7b040, false);
        auto request = TargetlessRequest(driver, opcode, 20, line.address);
        request.ReturnNID() = validNode ? 5 : 3;
        request.StashNIDValid() = validNode ? 1 : 0;
        request.StashLPIDValid() = validLpid ? 1 : 0;
        request.StashLPID() = validLpid ? 3 : 1;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        if (validNode)
        {
            const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
            driver.WriteData(4, 20, dbid.DBID(), TargetlessPattern(), FullTargetlessEnables(),
                CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
        }
        else
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::DERR);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.node.PeekTXSNP());
        LUNATIC_CHECK(!driver.node.InspectHome(line).valid);
        LUNATIC_CHECK(driver.node.GetMemory().Read(line) == LineData{});
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9TargetlessWriteUniqueFullStashAllocatesHome)
{
    CheckTargetlessWrite(CHI::Eb::Opcodes::REQ::WriteUniqueFullStash, false);
}

LUNATIC_TEST(Stage9TargetlessWriteUniquePtlStashAllocatesHome)
{
    CheckTargetlessWrite(CHI::Eb::Opcodes::REQ::WriteUniquePtlStash, false);
}

LUNATIC_TEST(Stage9TargetlessWriteUniqueFullStashSelectsUniquePeer)
{
    CheckTargetlessWrite(CHI::Eb::Opcodes::REQ::WriteUniqueFullStash, true);
}

LUNATIC_TEST(Stage9TargetlessWriteUniquePtlStashSelectsUniquePeer)
{
    CheckTargetlessWrite(CHI::Eb::Opcodes::REQ::WriteUniquePtlStash, true);
}

LUNATIC_TEST(Stage9TargetlessStashRejectsReservedAndInapplicableFields)
{
    CheckInvalidTarget(CHI::Eb::Opcodes::REQ::WriteUniqueFullStash, false, true);
    CheckInvalidTarget(CHI::Eb::Opcodes::REQ::WriteUniquePtlStash, false, false);
    CheckInvalidTarget(CHI::Eb::Opcodes::REQ::WriteUniqueFullStash, true, false);
}
