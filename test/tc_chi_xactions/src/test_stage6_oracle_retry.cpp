#include "sequence.hpp"

#include <memory>
#include <type_traits>
#include <vector>

using namespace XactTest;

namespace {

    template<typename Fixture>
    Flits::REQ<config> RetryRead(uint16_t txnId, uint64_t address = 0x10000)
    {
        auto request = Request(Opcodes::REQ::ReadNoSnp, txnId);
        request.Addr() = address;
        request.SnpAttr() = 0;
        request.MemAttr() = MemAttrs::NonCacheable_Bufferable;
        if constexpr (std::is_same_v<Fixture, SNFixture>)
        {
            request.SrcID() = 2;
            request.TgtID() = 6;
        }
        return request;
    }

    template<typename Fixture>
    Flits::RSP<config> RetryResponse(uint16_t opcode, uint16_t txnId, uint8_t creditType = 0)
    {
        if constexpr (std::is_same_v<Fixture, SNFixture>)
        {
            auto response = Response(6, 2, opcode, txnId);
            response.PCrdType() = creditType;
            return response;
        }
        else
        {
            auto response = Response(2, 4, opcode, txnId);
            response.PCrdType() = creditType;
            return response;
        }
    }

    template<typename Fixture>
    Xact::XactDenialEnum NextRequest(Fixture& fixture, const Flits::REQ<config>& request, uint64_t time)
    {
        if constexpr (std::is_same_v<Fixture, SNFixture>)
            return fixture.joint.NextRXREQ(fixture.global, time, request);
        else
            return fixture.joint.NextTXREQ(fixture.global, time, request);
    }

    template<typename Fixture>
    Xact::XactDenialEnum NextResponse(Fixture& fixture, const Flits::RSP<config>& response, uint64_t time)
    {
        if constexpr (std::is_same_v<Fixture, SNFixture>)
            return fixture.joint.NextTXRSP(fixture.global, time, response);
        else
            return fixture.joint.NextRXRSP(fixture.global, time, response);
    }

    template<typename Fixture>
    Xact::XactDenialEnum Retry(Fixture& fixture, uint16_t txnId, uint8_t creditType = 0)
    {
        return NextResponse(fixture, RetryResponse<Fixture>(Opcodes::RSP::RetryAck, txnId, creditType), 20);
    }

    template<typename Fixture>
    Xact::XactDenialEnum Grant(Fixture& fixture, uint8_t creditType = 0)
    {
        return NextResponse(fixture, RetryResponse<Fixture>(Opcodes::RSP::PCrdGrant, 0, creditType), 21);
    }

    template<typename Fixture>
    Xact::XactDenialEnum Return(Fixture& fixture, uint16_t txnId = 0, uint8_t creditType = 0)
    {
        Flits::REQ<config> request{};
        request.Opcode() = Opcodes::REQ::PCrdReturn;
        request.TxnID() = txnId;
        request.PCrdType() = creditType;
        if constexpr (std::is_same_v<Fixture, SNFixture>)
        {
            request.SrcID() = 2;
            request.TgtID() = 6;
        }
        else
        {
            request.SrcID() = 4;
            request.TgtID() = 2;
        }
        return NextRequest(fixture, request, 22);
    }

    template<typename Fixture>
    void CompleteRead(Fixture& fixture, uint16_t txnId)
    {
        for (uint8_t dataId : { uint8_t(0), uint8_t(2) })
        {
            Flits::DAT<config> data{};
            if constexpr (std::is_same_v<Fixture, SNFixture>)
            {
                data.SrcID() = 6;
                data.TgtID() = 2;
            }
            else
            {
                data.SrcID() = 2;
                data.TgtID() = 4;
                data.HomeNID() = 2;
            }
            data.TxnID() = txnId;
            data.Opcode() = Opcodes::DAT::CompData;
            data.Resp() = Resps::CompData_I;
            data.DataID() = dataId;
            const auto time = 30 + dataId;
            if constexpr (std::is_same_v<Fixture, SNFixture>)
                XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, time, data));
            else
                XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, time, data));
        }
    }

    template<typename Fixture>
    void RecordRetries(Fixture& fixture, std::vector<uint16_t>& selected)
    {
        XACT_CHECK(fixture.joint.events != nullptr);
        fixture.joint.events->OnRetry.Register(Gravity::MakeListener<Xact::JointXactionRetryEvent<config>>(
            "stage6-retry", 0, [&](Xact::JointXactionRetryEvent<config>& event) {
                auto original = event.GetXaction()->GetFirstTry();
                XACT_CHECK(original != nullptr);
                selected.push_back(original->GetFirst().flit.req.TxnID());
            }));
    }

    template<typename Fixture>
    void ChangedTxnId()
    {
        Fixture fixture;
        auto original = RetryRead<Fixture>(11);
        XACT_ACCEPT(NextRequest(fixture, original, 1));
        XACT_ACCEPT(Retry(fixture, 11));
        XACT_ACCEPT(Grant(fixture));
        auto resend = RetryRead<Fixture>(12);
        resend.AllowRetry() = 0;
        XACT_ACCEPT(NextRequest(fixture, resend, 23));
        CompleteRead(fixture, 12);
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    template<typename Fixture>
    void CreditReturn()
    {
        Fixture fixture;
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(21), 1));
        XACT_ACCEPT(Retry(fixture, 21));
        XACT_ACCEPT(Grant(fixture));
        XACT_ACCEPT(Return(fixture));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    template<typename Fixture>
    void ReuseOriginalTxnId()
    {
        Fixture fixture;
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(81, 0x10000), 1));
        XACT_ACCEPT(Retry(fixture, 81));

        auto replacement = RetryRead<Fixture>(81, 0x20000);
        XACT_ACCEPT(NextRequest(fixture, replacement, 2));
        XACT_ACCEPT(Grant(fixture));

        auto resend = RetryRead<Fixture>(82, 0x10000);
        resend.AllowRetry() = 0;
        XACT_ACCEPT(NextRequest(fixture, resend, 23));
        CompleteRead(fixture, 81);
        CompleteRead(fixture, 82);
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    template<typename Fixture>
    void YoungestCompatible()
    {
        Fixture fixture;
        std::vector<uint16_t> selected;
        RecordRetries(fixture, selected);
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(31), 1));
        XACT_ACCEPT(Retry(fixture, 31));
        XACT_ACCEPT(Grant(fixture));
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(32), 2));
        XACT_ACCEPT(Grant(fixture));
        XACT_ACCEPT(Retry(fixture, 32));

        auto first = RetryRead<Fixture>(33);
        first.AllowRetry() = 0;
        XACT_ACCEPT(NextRequest(fixture, first, 23));
        auto second = RetryRead<Fixture>(34);
        second.AllowRetry() = 0;
        XACT_ACCEPT(NextRequest(fixture, second, 24));
        XACT_CHECK((selected == std::vector<uint16_t>{ 32, 31 }));
        CompleteRead(fixture, 33);
        CompleteRead(fixture, 34);
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    template<typename Fixture>
    void SkipIncompatibleYounger()
    {
        Fixture fixture;
        std::vector<uint16_t> selected;
        RecordRetries(fixture, selected);
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(41, 0x10000), 1));
        XACT_ACCEPT(Retry(fixture, 41));
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(42, 0x20000), 2));
        XACT_ACCEPT(Retry(fixture, 42));
        XACT_ACCEPT(Grant(fixture));
        XACT_ACCEPT(Grant(fixture));

        auto older = RetryRead<Fixture>(43, 0x10000);
        older.AllowRetry() = 0;
        XACT_ACCEPT(NextRequest(fixture, older, 23));
        auto younger = RetryRead<Fixture>(44, 0x20000);
        younger.AllowRetry() = 0;
        XACT_ACCEPT(NextRequest(fixture, younger, 24));
        XACT_CHECK((selected == std::vector<uint16_t>{ 41, 42 }));
        CompleteRead(fixture, 43);
        CompleteRead(fixture, 44);
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    template<typename Fixture>
    void ReturnThenYoungestResend()
    {
        Fixture fixture;
        std::vector<uint16_t> selected;
        RecordRetries(fixture, selected);
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(51), 1));
        XACT_ACCEPT(Retry(fixture, 51));
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(52), 2));
        XACT_ACCEPT(Retry(fixture, 52));
        XACT_ACCEPT(Grant(fixture));
        XACT_ACCEPT(Grant(fixture));
        XACT_ACCEPT(Return(fixture));
        XACT_CHECK(fixture.joint.HasInflight());

        auto resend = RetryRead<Fixture>(53);
        resend.AllowRetry() = 0;
        XACT_ACCEPT(NextRequest(fixture, resend, 23));
        CompleteRead(fixture, 53);
        XACT_CHECK((selected == std::vector<uint16_t>{ 52 }));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    template<typename Fixture>
    void ExcessCreditReturn()
    {
        Fixture fixture;
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(61), 1));
        XACT_ACCEPT(Retry(fixture, 61));
        XACT_ACCEPT(Grant(fixture));
        XACT_ACCEPT(Grant(fixture));
        auto resend = RetryRead<Fixture>(62);
        resend.AllowRetry() = 0;
        XACT_ACCEPT(NextRequest(fixture, resend, 23));
        CompleteRead(fixture, 62);
        XACT_CHECK(fixture.joint.HasInflight());
        XACT_ACCEPT(Return(fixture));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    template<typename Fixture>
    void RejectedResendPreservesCredit()
    {
        Fixture fixture;
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(71, 0x10000), 1));
        XACT_ACCEPT(Retry(fixture, 71));
        XACT_ACCEPT(Grant(fixture));
        auto wrong = RetryRead<Fixture>(72, 0x20000);
        wrong.AllowRetry() = 0;
        XACT_CHECK(NextRequest(fixture, wrong, 23) != XactDenial::ACCEPTED);
        XACT_CHECK(fixture.joint.HasInflight());
        auto correct = RetryRead<Fixture>(73, 0x10000);
        correct.AllowRetry() = 0;
        XACT_ACCEPT(NextRequest(fixture, correct, 24));
        CompleteRead(fixture, 73);
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    template<typename Fixture>
    void InvalidAndDuplicateReturn()
    {
        Fixture fixture;
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(91), 1));
        XACT_ACCEPT(Retry(fixture, 91));
        XACT_ACCEPT(Grant(fixture));
        XACT_CHECK(Return(fixture, 1) != XactDenial::ACCEPTED);
        XACT_CHECK(fixture.joint.HasInflight());
        XACT_ACCEPT(Return(fixture));
        XACT_CHECK(!fixture.joint.HasInflight());
        XACT_CHECK(Return(fixture) != XactDenial::ACCEPTED);
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    template<typename Fixture>
    void CreditTypes()
    {
        Fixture fixture;
        std::vector<uint16_t> selected;
        RecordRetries(fixture, selected);
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(111), 1));
        XACT_ACCEPT(Retry(fixture, 111, 1));
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(112), 2));
        XACT_ACCEPT(Retry(fixture, 112, 2));
        XACT_ACCEPT(Grant(fixture, 2));
        XACT_ACCEPT(Grant(fixture, 1));

        auto second = RetryRead<Fixture>(113);
        second.AllowRetry() = 0;
        second.PCrdType() = 2;
        XACT_ACCEPT(NextRequest(fixture, second, 23));
        auto first = RetryRead<Fixture>(114);
        first.AllowRetry() = 0;
        first.PCrdType() = 1;
        XACT_ACCEPT(NextRequest(fixture, first, 24));
        XACT_CHECK((selected == std::vector<uint16_t>{ 112, 111 }));
        CompleteRead(fixture, 113);
        CompleteRead(fixture, 114);
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    template<typename Fixture>
    void InvalidGrantPreservesCreditState()
    {
        Fixture fixture;
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(121), 1));
        XACT_ACCEPT(Retry(fixture, 121));
        auto invalid = RetryResponse<Fixture>(Opcodes::RSP::PCrdGrant, 1);
        XACT_CHECK(NextResponse(fixture, invalid, 21) != XactDenial::ACCEPTED);
        XACT_CHECK(fixture.joint.HasInflight());
        XACT_ACCEPT(Grant(fixture));
        XACT_ACCEPT(Return(fixture));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    template<typename Fixture>
    void GrantBeforeRetryAck()
    {
        Fixture fixture;
        XACT_ACCEPT(NextRequest(fixture, RetryRead<Fixture>(101), 1));
        XACT_ACCEPT(Grant(fixture));
        XACT_ACCEPT(Retry(fixture, 101));
        XACT_ACCEPT(Return(fixture));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

}

XACT_TEST(Stage6OracleRNChangedTxnIdResend) { ChangedTxnId<RNFixture>(); }
XACT_TEST(Stage6OracleSNChangedTxnIdResend) { ChangedTxnId<SNFixture>(); }
XACT_TEST(Stage6OracleRNCreditReturn) { CreditReturn<RNFixture>(); }
XACT_TEST(Stage6OracleSNCreditReturn) { CreditReturn<SNFixture>(); }
XACT_TEST(Stage6OracleRNOriginalTxnIdReuse) { ReuseOriginalTxnId<RNFixture>(); }
XACT_TEST(Stage6OracleSNOriginalTxnIdReuse) { ReuseOriginalTxnId<SNFixture>(); }
XACT_TEST(Stage6OracleRNYoungestCompatibleRetry) { YoungestCompatible<RNFixture>(); }
XACT_TEST(Stage6OracleSNYoungestCompatibleRetry) { YoungestCompatible<SNFixture>(); }
XACT_TEST(Stage6OracleRNSkipIncompatibleYoungerRetry) { SkipIncompatibleYounger<RNFixture>(); }
XACT_TEST(Stage6OracleSNSkipIncompatibleYoungerRetry) { SkipIncompatibleYounger<SNFixture>(); }
XACT_TEST(Stage6OracleRNReturnThenYoungestResend) { ReturnThenYoungestResend<RNFixture>(); }
XACT_TEST(Stage6OracleSNReturnThenYoungestResend) { ReturnThenYoungestResend<SNFixture>(); }
XACT_TEST(Stage6OracleRNExcessCreditReturn) { ExcessCreditReturn<RNFixture>(); }
XACT_TEST(Stage6OracleSNExcessCreditReturn) { ExcessCreditReturn<SNFixture>(); }
XACT_TEST(Stage6OracleRNRejectedResendPreservesCredit) { RejectedResendPreservesCredit<RNFixture>(); }
XACT_TEST(Stage6OracleSNRejectedResendPreservesCredit) { RejectedResendPreservesCredit<SNFixture>(); }
XACT_TEST(Stage6OracleRNInvalidAndDuplicateReturn) { InvalidAndDuplicateReturn<RNFixture>(); }
XACT_TEST(Stage6OracleSNInvalidAndDuplicateReturn) { InvalidAndDuplicateReturn<SNFixture>(); }
XACT_TEST(Stage6OracleRNGrantBeforeRetryAck) { GrantBeforeRetryAck<RNFixture>(); }
XACT_TEST(Stage6OracleSNGrantBeforeRetryAck) { GrantBeforeRetryAck<SNFixture>(); }
XACT_TEST(Stage6OracleRNCreditTypes) { CreditTypes<RNFixture>(); }
XACT_TEST(Stage6OracleSNCreditTypes) { CreditTypes<SNFixture>(); }
XACT_TEST(Stage6OracleRNInvalidGrantPreservesCreditState) { InvalidGrantPreservesCreditState<RNFixture>(); }
XACT_TEST(Stage6OracleSNInvalidGrantPreservesCreditState) { InvalidGrantPreservesCreditState<SNFixture>(); }
