#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    ByteEnables SLCRepHintEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    template<typename config>
    void ForceSLCRepHintRetry(CheckedDriver<config>& driver)
    {
        driver.driver.node.GetEvents()->OnRequestAdmission.Register(
            Gravity::MakeListener<RequestAdmissionEvent>("slc-rephint-retry", 0, [&](RequestAdmissionEvent& event) {
                if (event.request.allowRetry)
                    event.RequestRetry();
            }));
    }

    void CheckSLCRepHintResponse(const CheckedDriver<Config256>::RSP& response)
    {
        LUNATIC_CHECK(response.QoS() == 12);
        LUNATIC_CHECK(response.TraceTag() == 1);
    }

    void RunSLCRepHintRetry(uint16_t opcode)
    {
        Options options;
        options.outputQueueLimit = 1;
        CheckedDriver<Config256> driver(options);
        ForceSLCRepHintRetry(driver);
        auto request = driver.Request(opcode, 4, 2, 0x62000);
        if (opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep)
            request.PGroupID() = 0x44;
        driver.PushRequest(request);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck).TxnID() == 2);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

        request.TxnID() = 9;
        request.AllowRetry() = 0;
        request.ReturnNID() = 1;
        request.QoS() = 12;
        request.TraceTag() = 1;
        driver.PushRequest(request);

        if (opcode == CHI::Eb::Opcodes::REQ::ReadNoSnp)
        {
            for (size_t beat = 0; beat < 2; ++beat)
            {
                const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
                LUNATIC_CHECK(data.TxnID() == 9 && data.QoS() == 12 && data.TraceTag() == 1);
            }
        }
        else if (opcode == CHI::Eb::Opcodes::REQ::WriteNoSnpFull)
        {
            const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
            CheckSLCRepHintResponse(dbid);
            driver.WriteData(4, 9, dbid.DBID(), LineData{}, SLCRepHintEnables(),
                CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
            CheckSLCRepHintResponse(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp));
        }
        else if (opcode == CHI::Eb::Opcodes::REQ::WriteBackFull)
        {
            const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
            CheckSLCRepHintResponse(dbid);
            driver.WriteData(4, 9, dbid.DBID(), LineData{}, SLCRepHintEnables(),
                CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        }
        else if (opcode == CHI::Eb::Opcodes::REQ::CleanShared)
            CheckSLCRepHintResponse(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp));
        else
        {
            LUNATIC_CHECK(opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
            const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompPersist);
            CheckSLCRepHintResponse(response);
            LUNATIC_CHECK(response.PGroupID() == 0x44);
        }
        LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().creditedResends == 1);
        driver.Quiesce();
    }

    template<typename config>
    void CompleteSLCRepHintRead(CheckedDriver<config>& driver, TransactionId txnId)
    {
        for (size_t beat = 0; beat < 2; ++beat)
            LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).TxnID() == txnId);
    }

}

LUNATIC_TEST(Stage9SLCRepHintRetryFamilies)
{
    for (uint16_t opcode : { CHI::Eb::Opcodes::REQ::WriteBackFull, CHI::Eb::Opcodes::REQ::ReadNoSnp,
             CHI::Eb::Opcodes::REQ::WriteNoSnpFull, CHI::Eb::Opcodes::REQ::CleanShared,
             CHI::Eb::Opcodes::REQ::CleanSharedPersistSep })
        RunSLCRepHintRetry(opcode);
}

LUNATIC_TEST(Stage9SLCRepHintMultipleOriginals)
{
    Options options;
    options.transactionLimit = 2;
    options.outputQueueLimit = 1;
    CheckedDriver<Config256> driver(options);
    ForceSLCRepHintRetry(driver);
    auto older = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 3, 0x62100, false);
    auto younger = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 4, 0x62140, false);
    older.ReturnNID() = 0;
    younger.ReturnNID() = 1;
    driver.PushRequest(older);
    driver.driver.Tick();
    driver.PushRequest(younger);
    driver.driver.Tick();
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck).TxnID() == 3);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck).TxnID() == 4);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

    younger.TxnID() = 5;
    younger.AllowRetry() = 0;
    younger.ReturnNID() = 3;
    driver.PushRequest(younger);
    CompleteSLCRepHintRead(driver, 5);
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().creditedResends == 1);

    CheckedDriver<Config256>::REQ creditReturn{};
    creditReturn.SrcID() = 4;
    creditReturn.TgtID() = 2;
    creditReturn.Opcode() = CHI::Eb::Opcodes::REQ::PCrdReturn;
    creditReturn.PCrdType() = older.PCrdType();
    driver.PushRequest(creditReturn);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().returnedCredits == 1);
}

LUNATIC_TEST(Stage9SLCRepHintCompatibilityControls)
{
    Request original{};
    original.source = 4;
    original.target = 2;
    original.opcode = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    original.returnNid = 0;
    original.allowRetry = true;
    Request resend = original;
    resend.allowRetry = false;
    resend.returnNid = 1;
    LUNATIC_CHECK(RetryRequestCompatible(original, resend, 2));

    original.opcode = CHI::Eb::Opcodes::REQ::WriteUniquePtlStash;
    original.stashNidValid = true;
    original.returnNid = 5;
    resend = original;
    resend.allowRetry = false;
    resend.returnNid = 6;
    LUNATIC_CHECK(!RetryRequestCompatible(original, resend, 2));

    original.opcode = CHI::Eb::Opcodes::REQ::AtomicStore::ADD;
    original.stashNidValid = false;
    original.returnNid = 0;
    resend = original;
    resend.allowRetry = false;
    resend.returnNid = 1;
    LUNATIC_CHECK(!RetryRequestCompatible(original, resend, 2));
}
