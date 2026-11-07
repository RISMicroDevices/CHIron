#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    Driver<Config256>::REQ PrefetchRequest(Driver<Config256>& driver, TransactionId txnId)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::PrefetchTgt, 4, txnId, 0x21000);
        request.Size() = 7;
        request.Order() = 3;
        request.MemAttr() = 0x0f;
        request.AllowRetry() = 1;
        return request;
    }

    void CheckNoTraffic(Driver<Config256>& driver)
    {
        for (size_t cycle = 0; cycle < 12; ++cycle)
        {
            LUNATIC_CHECK(!driver.node.PeekTXRSP());
            LUNATIC_CHECK(!driver.node.PeekTXREQ());
            LUNATIC_CHECK(!driver.node.PeekTXSNP());
            LUNATIC_CHECK(!driver.node.PeekTXDAT());
            driver.Tick();
        }
        LUNATIC_CHECK(driver.node.GetMemory().Read(LineKey::FromAddress(0x21000, false)) == LineData{});
        LUNATIC_CHECK(!driver.node.InspectHome(LineKey::FromAddress(0x21000, false)).valid);
        LUNATIC_CHECK(driver.node.InspectRequester(LineKey::FromAddress(0x21000, false), 4) == CacheState::I);
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void CompleteReadControl(Driver<Config256>& driver, TransactionId txnId, uint64_t address)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, txnId, address)));
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TxnID() == txnId);
        }
    }

}

LUNATIC_TEST(Stage9PrefetchTgtDiscardedWithoutResponse)
{
    Driver<Config256> driver;
    driver.node.PushRXREQ(PrefetchRequest(driver, 0x0fff));
    CheckNoTraffic(driver);
    LUNATIC_CHECK(!driver.node.HasInflight());
    CompleteReadControl(driver, 1, 0x21040);
    driver.Quiesce();
}

LUNATIC_TEST(Stage9PrefetchTgtDiscardedWithForcedRetry)
{
    Driver<Config256> driver;
    driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("prefetch-retry", 0, [&](RequestAdmissionEvent& event) {
            if (event.request.opcode == CHI::Eb::Opcodes::REQ::PrefetchTgt)
                event.RequestRetry();
        }));
    driver.node.PushRXREQ(PrefetchRequest(driver, 0x0fff));
    CheckNoTraffic(driver);
    LUNATIC_CHECK(!driver.node.HasInflight());
    CompleteReadControl(driver, 1, 0x21040);
    driver.Quiesce();
}

LUNATIC_TEST(Stage9PrefetchTgtDiscardedAtCapacity)
{
    Options options;
    options.transactionLimit = 1;
    Driver<Config256> driver(options);
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x21080, true)));
    const auto finalData = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);

    driver.node.PushRXREQ(PrefetchRequest(driver, 1));
    CheckNoTraffic(driver);
    LUNATIC_CHECK(driver.node.HasInflight());
    driver.CompAck(4, finalData.DBID());
    driver.Quiesce();
    CompleteReadControl(driver, 2, 0x210c0);
    driver.Quiesce();
}
