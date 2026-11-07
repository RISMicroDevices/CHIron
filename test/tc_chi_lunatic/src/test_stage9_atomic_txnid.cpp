#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    Driver<Config256>::DAT AtomicOperand(const Driver<Config256>::REQ& request, DataBufferId dbid)
    {
        Driver<Config256>::DAT data{};
        data.SrcID() = 4;
        data.TgtID() = 2;
        data.TxnID() = dbid;
        data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        data.DataID() = ((request.Addr() % LINE_SIZE) / 32) * 2;
        data.CCID() = (request.Addr() >> 4) & 3;
        data.BE() = 0b1111;
        for (size_t index = 0; index < 4; ++index)
            data.Data()[0] |= uint64_t(index + 1) << (index * 8);
        return data;
    }

    void SendAtomicOperand(CheckedDriver<Config256>& driver, const Driver<Config256>::REQ& request,
                           DataBufferId dbid)
    {
        const auto data = AtomicOperand(request, dbid);
        driver.Oracle(4).WriteData(driver.driver.now, data);
        LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
        driver.driver.Tick();
    }

    void CompleteReplacementRead(CheckedDriver<Config256>& driver, DataBufferId oldDBID)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x1c100));
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TxnID() == 7);
            LUNATIC_CHECK(data.DBID() != oldDBID);
        }
    }

}

LUNATIC_TEST(Stage9AtomicTxnIDStoreReleasesTxnIDAfterComp)
{
    CheckedDriver<Config256> driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 4, 7, 0x1c020);
    request.Size() = 2;
    driver.PushRequest(request);
    const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    SendAtomicOperand(driver, request, dbid);
    const auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(comp.DBID() == dbid);

    CompleteReplacementRead(driver, dbid);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().admittedTransactions == 2);
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().retiredTransactions == 2);
}

LUNATIC_TEST(Stage9AtomicTxnIDSwapReleasesTxnIDAfterResultData)
{
    CheckedDriver<Config256> driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicSwap, 4, 7, 0x1c040);
    request.Size() = 2;
    driver.PushRequest(request);
    const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    SendAtomicOperand(driver, request, dbid);
    const auto result = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(result.DBID() == dbid);

    CompleteReplacementRead(driver, dbid);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().admittedTransactions == 2);
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().retiredTransactions == 2);
}

LUNATIC_TEST(Stage9AtomicTxnIDStorePrematureTxnIDReuseIsRejected)
{
    Driver<Config256> driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 4, 7, 0x1c080);
    request.Size() = 2;
    driver.node.PushRXREQ(request);
    const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();

    driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x1c180));
    driver.Tick();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 1);
    LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    LUNATIC_CHECK(driver.node.GetErrors().front().reason.find("duplicate live requester transaction ID") !=
                  std::string::npos);

    const auto operand = AtomicOperand(request, dbid);
    driver.node.PushRXDAT(operand);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
}
