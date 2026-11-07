#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    void CompleteReplacementRead(CheckedDriver<Config256>& driver, DataBufferId oldDBID)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x1d100));
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TxnID() == 7);
            LUNATIC_CHECK(data.DBID() != oldDBID);
        }
    }

    void RunDatalessTxnIDRelease(uint16_t opcode)
    {
        CheckedDriver<Config256> driver;
        const bool compAck = opcode == CHI::Eb::Opcodes::REQ::MakeUnique ||
            opcode == CHI::Eb::Opcodes::REQ::CleanUnique;
        auto request = driver.Request(opcode, 4, 7, 0x1d000, compAck);
        if (opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep)
            request.PGroupID() = 0x44;
        if (opcode == CHI::Eb::Opcodes::REQ::StashOnceUnique)
        {
            request.ReturnNID() = 5;
            request.StashNIDValid() = 1;
            request.StashLPIDValid() = 1;
            request.StashLPID() = 3;
        }
        driver.PushRequest(request);
        if (opcode == CHI::Eb::Opcodes::REQ::StashOnceUnique)
        {
            const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpStashUnique);
            driver.SnoopResponse(5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        }

        const auto response = driver.NextRSP(opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep
            ? CHI::Eb::Opcodes::RSP::CompPersist : CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(response.TxnID() == 7);
        CompleteReplacementRead(driver, response.DBID());
        if (compAck)
            driver.CompAck(4, response.DBID());
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().admittedTransactions == 2);
        LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().retiredTransactions == 2);
    }

}

LUNATIC_TEST(Stage9DatalessTxnIDMakeUnique)
{
    RunDatalessTxnIDRelease(CHI::Eb::Opcodes::REQ::MakeUnique);
}

LUNATIC_TEST(Stage9DatalessTxnIDCleanShared)
{
    RunDatalessTxnIDRelease(CHI::Eb::Opcodes::REQ::CleanShared);
}

LUNATIC_TEST(Stage9DatalessTxnIDCleanUnique)
{
    RunDatalessTxnIDRelease(CHI::Eb::Opcodes::REQ::CleanUnique);
}

LUNATIC_TEST(Stage9DatalessTxnIDEvict)
{
    RunDatalessTxnIDRelease(CHI::Eb::Opcodes::REQ::Evict);
}

LUNATIC_TEST(Stage9DatalessTxnIDStashOnce)
{
    RunDatalessTxnIDRelease(CHI::Eb::Opcodes::REQ::StashOnceUnique);
}

LUNATIC_TEST(Stage9DatalessTxnIDCleanSharedPersist)
{
    RunDatalessTxnIDRelease(CHI::Eb::Opcodes::REQ::CleanSharedPersist);
}

LUNATIC_TEST(Stage9DatalessTxnIDCompPersist)
{
    RunDatalessTxnIDRelease(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
}
