#include "driver.hpp"

#include <functional>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    Driver<Config256>::REQ GrantRetryCredit(Driver<Config256>& driver)
    {
        const auto first = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x60000, false);
        LUNATIC_CHECK(driver.node.PushRXREQ(first));
        driver.Tick();
        const auto retried = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x60040, false);
        LUNATIC_CHECK(driver.node.PushRXREQ(retried));
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck).TxnID() == 2);
        for (size_t beat = 0; beat < 2; ++beat)
            LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).TxnID() == 1);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
        return retried;
    }

    Driver<Config256>::REQ CreditReturn(const Driver<Config256>::REQ& retried)
    {
        Driver<Config256>::REQ request{};
        request.SrcID() = 4;
        request.TgtID() = 2;
        request.Opcode() = CHI::Eb::Opcodes::REQ::PCrdReturn;
        request.PCrdType() = retried.PCrdType();
        request.QoS() = 7;
        request.TraceTag() = 1;
        return request;
    }

    void RunCreditReturnControl()
    {
        Options options;
        options.transactionLimit = 1;
        Driver<Config256> driver(options);
        const auto retried = GrantRetryCredit(driver);
        LUNATIC_CHECK(driver.node.PushRXREQ(CreditReturn(retried)));
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetPerformanceCounters().returnedCredits == 1);
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void RunMalformedCreditReturn(const std::function<void(Driver<Config256>::REQ&)>& invalidate)
    {
        Options options;
        options.transactionLimit = 1;
        Driver<Config256> driver(options);
        const auto retried = GrantRetryCredit(driver);
        auto malformed = CreditReturn(retried);
        invalidate(malformed);
        LUNATIC_CHECK(driver.node.PushRXREQ(malformed));
        driver.Tick();
        LUNATIC_CHECK(driver.node.GetPerformanceCounters().returnedCredits == 0);
        LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
        LUNATIC_CHECK(!driver.node.PeekTXRSP());
        LUNATIC_CHECK(driver.node.HasInflight());

        LUNATIC_CHECK(driver.node.PushRXREQ(CreditReturn(retried)));
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetPerformanceCounters().returnedCredits == 1);
        LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    }

}

LUNATIC_TEST(Stage9PCrdReturnValidControl)
{
    RunCreditReturnControl();
}

LUNATIC_TEST(Stage9PCrdReturnRejectsAllowRetry)
{
    RunMalformedCreditReturn([](Driver<Config256>::REQ& request) {
        request.AllowRetry() = 1;
    });
}

LUNATIC_TEST(Stage9PCrdReturnRejectsSize)
{
    RunMalformedCreditReturn([](Driver<Config256>::REQ& request) {
        request.Size() = 6;
    });
}

LUNATIC_TEST(Stage9PCrdReturnValidatesInapplicableFields)
{
    const std::vector<std::function<void(Driver<Config256>::REQ&)>> invalidFields{
        [](Driver<Config256>::REQ& request) { request.TxnID() = 1; },
        [](Driver<Config256>::REQ& request) { request.ReturnNID() = 3; },
        [](Driver<Config256>::REQ& request) { request.ReturnTxnID() = 4; },
        [](Driver<Config256>::REQ& request) { request.Addr() = 0x40; },
        [](Driver<Config256>::REQ& request) { request.NS() = 1; },
        [](Driver<Config256>::REQ& request) { request.MemAttr() = 1; },
        [](Driver<Config256>::REQ& request) { request.SnpAttr() = 1; },
        [](Driver<Config256>::REQ& request) { request.Order() = 2; },
        [](Driver<Config256>::REQ& request) { request.LikelyShared() = 1; },
        [](Driver<Config256>::REQ& request) { request.ExpCompAck() = 1; },
        [](Driver<Config256>::REQ& request) { request.StashNIDValid() = 1; },
        [](Driver<Config256>::REQ& request) { request.StashLPIDValid() = 1; },
        [](Driver<Config256>::REQ& request) { request.StashLPID() = 3; },
        [](Driver<Config256>::REQ& request) { request.PGroupID() = 2; },
        [](Driver<Config256>::REQ& request) { request.StashGroupID() = 2; },
        [](Driver<Config256>::REQ& request) { request.Endian() = 1; },
        [](Driver<Config256>::REQ& request) { request.Deep() = 1; },
        [](Driver<Config256>::REQ& request) { request.DoDWT() = 1; },
        [](Driver<Config256>::REQ& request) { request.SnoopMe() = 1; }
    };
    for (const auto& invalidate : invalidFields)
        RunMalformedCreditReturn(invalidate);
}

LUNATIC_TEST(Stage9PCrdReturnConstructorFields)
{
    const auto request = MakePCrdReturnRequest(4, 2, 3, 7, 1);
    LUNATIC_CHECK(IsPCrdReturnValid(request));
    LUNATIC_CHECK(request.source == 4 && request.target == 2);
    LUNATIC_CHECK(request.opcode == CHI::Eb::Opcodes::REQ::PCrdReturn);
    LUNATIC_CHECK(request.pCrdType == 3 && request.qos == 7 && request.traceTag == 1);
    LUNATIC_CHECK(request.size == 0 && !request.allowRetry);
}
