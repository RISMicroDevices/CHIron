#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData StashCompData()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(23 + index * 5);
        return data;
    }

    void SeedStashCompHome(Driver<Config256>& driver, const LineKey& line, TransactionId txnId)
    {
        ByteEnables enables{};
        enables.fill(1);
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, txnId, line.address)));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, txnId, response.DBID(), StashCompData(), enables,
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.InspectHome(line).valid);
    }

    Driver<Config256>::REQ StashCompRequest(Driver<Config256>& driver, uint16_t opcode,
        TransactionId txnId, uint64_t address, uint8_t group = 0)
    {
        auto request = driver.Request(opcode, 4, txnId, address);
        request.ReturnNID() = 5;
        request.StashNIDValid() = 1;
        request.StashLPIDValid() = 1;
        request.StashLPID() = 3;
        if (IsSeparatedStashRequest(opcode))
            request.StashGroupID() = group;
        return request;
    }

    void RespondStashCompSnoop(Driver<Config256>& driver, bool unique)
    {
        const uint16_t opcode = unique ? CHI::Eb::Opcodes::SNP::SnpStashUnique
            : CHI::Eb::Opcodes::SNP::SnpStashShared;
        const auto snoop = driver.NextSNP(opcode);
        LUNATIC_CHECK(snoop.target == 5);
        Driver<Config256>::RSP response{};
        response.SrcID() = 5;
        response.TgtID() = 2;
        response.TxnID() = snoop.flit.TxnID();
        response.Opcode() = CHI::Eb::Opcodes::RSP::SnpResp;
        response.Resp() = CHI::Eb::Resps::SnpResp_I;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    void CheckStashCompDrain(Driver<Config256>& driver, const LineKey& line, bool homeHit)
    {
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.ValidateInvariants());
        LUNATIC_CHECK(driver.node.GetErrors().empty());
        LUNATIC_CHECK(driver.node.InspectHome(line).valid == homeHit);
        LUNATIC_CHECK(driver.node.InspectRequester(line, 5) == CacheState::I);
    }

    void RunIndependentStashComp(uint16_t opcode, bool homeHit)
    {
        Options options;
        options.outputQueueLimit = 1;
        Driver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x5f000, false);
        if (homeHit)
            SeedStashCompHome(driver, line, 90);
        driver.node.PushRXREQ(StashCompRequest(driver, opcode, 9, line.address));
        RespondStashCompSnoop(driver, opcode == CHI::Eb::Opcodes::REQ::StashOnceUnique);
        const auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(comp.TxnID() == 9);
        LUNATIC_CHECK(comp.Resp() == (homeHit ? CHI::Eb::Resps::Comp_SC : CHI::Eb::Resps::Comp_I));
        CheckStashCompDrain(driver, line, homeHit);
    }

    void RunSeparatedStashComp(uint16_t opcode, bool homeHit)
    {
        Options options;
        options.outputQueueLimit = 1;
        Driver<Config256> driver(options);
        if (homeHit)
            for (size_t form = 0; form < 3; ++form)
                SeedStashCompHome(driver, LineKey::FromAddress(0x5f100 + form * LINE_SIZE, false), 90 + form);

        for (size_t form = 0; form < 3; ++form)
        {
            const LineKey line = LineKey::FromAddress(0x5f100 + form * LINE_SIZE, false);
            const TransactionId txnId = 0x20 + form;
            const uint8_t group = 0x40 + form;
            LUNATIC_CHECK(driver.node.PushRXREQ(StashCompRequest(driver, opcode, txnId, line.address, group)));
            RespondStashCompSnoop(driver, opcode == CHI::Eb::Opcodes::REQ::StashOnceSepUnique);
            const uint8_t expectedResp = homeHit ? CHI::Eb::Resps::Comp_SC : CHI::Eb::Resps::Comp_I;
            if (form == 0)
            {
                const auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
                LUNATIC_CHECK(comp.TxnID() == txnId && comp.Resp() == expectedResp);
                const auto done = driver.NextRSP(CHI::Eb::Opcodes::RSP::StashDone);
                LUNATIC_CHECK(done.TxnID() == 0 && done.StashGroupID() == group);
            }
            else if (form == 1)
            {
                const auto done = driver.NextRSP(CHI::Eb::Opcodes::RSP::StashDone);
                LUNATIC_CHECK(done.TxnID() == 0 && done.StashGroupID() == group);
                const auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
                LUNATIC_CHECK(comp.TxnID() == txnId && comp.Resp() == expectedResp);
            }
            else
            {
                const auto done = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompStashDone);
                LUNATIC_CHECK(done.TxnID() == txnId && done.StashGroupID() == group);
                LUNATIC_CHECK(done.Resp() == expectedResp);
            }
            CheckStashCompDrain(driver, line, homeHit);
        }
    }

}

LUNATIC_TEST(Stage9IndependentStashCompHomeHit)
{
    RunIndependentStashComp(CHI::Eb::Opcodes::REQ::StashOnceUnique, true);
    RunIndependentStashComp(CHI::Eb::Opcodes::REQ::StashOnceShared, true);
}

LUNATIC_TEST(Stage9IndependentStashCompHomeMiss)
{
    RunIndependentStashComp(CHI::Eb::Opcodes::REQ::StashOnceUnique, false);
    RunIndependentStashComp(CHI::Eb::Opcodes::REQ::StashOnceShared, false);
}

LUNATIC_TEST(Stage9SeparatedStashCompHomeHit)
{
    RunSeparatedStashComp(CHI::Eb::Opcodes::REQ::StashOnceSepUnique, true);
    RunSeparatedStashComp(CHI::Eb::Opcodes::REQ::StashOnceSepShared, true);
}

LUNATIC_TEST(Stage9SeparatedStashCompHomeMiss)
{
    RunSeparatedStashComp(CHI::Eb::Opcodes::REQ::StashOnceSepUnique, false);
    RunSeparatedStashComp(CHI::Eb::Opcodes::REQ::StashOnceSepShared, false);
}
