#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    void CompleteRead(Driver<config>& driver, TransactionId txnId)
    {
        for (size_t beat = 0; beat < BeatsPerLine(driver.node.GetOptions()); ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TxnID() == txnId);
        }
    }

}

LUNATIC_TEST(Stage8FlowAdmissionBackpressureReleases)
{
    Driver<Config256> driver;
    bool hold = true;
    size_t blocked = 0;
    driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("backpressure", 0, [&](RequestAdmissionEvent& event) {
            if (hold)
            {
                ++blocked;
                event.Backpressure();
            }
        }));

    driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x10000, false));
    for (size_t cycle = 0; cycle < 3; ++cycle)
    {
        driver.Tick();
        LUNATIC_CHECK(!driver.node.PeekTXDAT());
        LUNATIC_CHECK(!driver.node.Drain());
    }
    LUNATIC_CHECK(blocked >= 3);

    hold = false;
    CompleteRead(driver, 7);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage8FlowScheduleDelayLatchesOnce)
{
    Driver<Config256> driver;
    size_t blocked = 2;
    size_t delayed = 0;
    driver.node.GetEvents()->OnTransactionSchedule.Register(
        Gravity::MakeListener<TransactionScheduleEvent>("schedule", 0, [&](TransactionScheduleEvent& event) {
            if (event.phase != Phase::Receive)
                return;
            if (blocked)
            {
                --blocked;
                event.Backpressure();
                return;
            }
            ++delayed;
            event.AddDelay(3);
        }));

    driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x10000, false));
    CompleteRead(driver, 7);
    driver.Quiesce();
    LUNATIC_CHECK(blocked == 0);
    LUNATIC_CHECK(delayed == 1);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage8FlowDecisionReentryIsRejected)
{
    Driver<Config256> driver;
    bool admission = true;
    bool schedule = true;
    driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("admission-reentry", 0, [&](RequestAdmissionEvent& event) {
            if (admission)
            {
                admission = false;
                driver.node.Tick(1000);
                LUNATIC_CHECK(event.request.txnId == 7);
            }
        }));
    driver.node.GetEvents()->OnTransactionSchedule.Register(
        Gravity::MakeListener<TransactionScheduleEvent>("schedule-reentry", 0, [&](TransactionScheduleEvent& event) {
            if (schedule && event.phase == Phase::Receive)
            {
                schedule = false;
                LUNATIC_CHECK(!driver.node.PopTXDAT());
            }
        }));

    driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x10000, false));
    CompleteRead(driver, 7);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetTime() < 1000);
    LUNATIC_CHECK(driver.node.ValidateInvariants());
    LUNATIC_CHECK(driver.node.GetErrors().size() == 2);
}
