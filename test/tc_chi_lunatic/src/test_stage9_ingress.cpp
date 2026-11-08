#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using REQ = TestDriver::REQ;
    using RSP = TestDriver::RSP;
    using DAT = TestDriver::DAT;

    LineData IngressPattern()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(0x4d + index * 21);
        return data;
    }

    DAT MakeData(NodeId source, NodeId target, NodeId home, TransactionId txnId,
                 uint16_t opcode, uint8_t resp, size_t beat, uint8_t ccid, const LineData& line)
    {
        DAT data{};
        data.SrcID() = source;
        data.TgtID() = target;
        data.HomeNID() = home;
        data.TxnID() = txnId;
        data.Opcode() = opcode;
        data.Resp() = resp;
        data.DataID() = beat * 2;
        data.CCID() = ccid;
        data.BE() = ~uint64_t(0);
        for (size_t index = 0; index < Config256::dataWidth / 8; ++index)
            data.Data()[index / 8] |= uint64_t(line[beat * (Config256::dataWidth / 8) + index]) << ((index % 8) * 8);
        return data;
    }

    DAT MakeReadData(const REQ& request, NodeId target, NodeId home, size_t beat, const LineData& line)
    {
        return MakeData(6, target, home, request.TxnID(), CHI::Eb::Opcodes::DAT::CompData,
            CHI::Eb::Resps::CompData_I, beat, (request.Addr() >> 4) & 3, line);
    }

    void SendReadData(TestDriver& driver, const REQ& request, const LineData& line)
    {
        for (size_t beat = 0; beat < 2; ++beat)
            LUNATIC_CHECK(driver.node.PushRXDAT(MakeReadData(request, 2, 2, beat, line)));
        driver.Tick();
    }

    void SeedDirtyProvider(TestDriver& driver, const LineKey& line, const LineData& data)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, line.address)));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        ByteEnables enables{};
        enables.fill(1);
        driver.WriteData(6, 90, response.DBID(), data, enables, CHI::Eb::Opcodes::DAT::CopyBackWrData,
            CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(6, 91, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        LUNATIC_CHECK(driver.node.InspectRequester(line, 6) == CacheState::UD);
    }

}

LUNATIC_TEST(Stage9IngressRejectsMisdirectedRequest)
{
    TestDriver driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x78000, false);
    request.TgtID() = 3;
    LUNATIC_CHECK(!driver.node.PushRXREQ(request));
    driver.Tick();
    LUNATIC_CHECK(!driver.node.HasInflight());
    LUNATIC_CHECK(!driver.node.PeekTXDAT());
    LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
}

LUNATIC_TEST(Stage9IngressRejectsMisdirectedCreditReturn)
{
    TestDriver driver;
    driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("force-retry", 0, [&](RequestAdmissionEvent& event) {
            if (event.request.allowRetry && event.request.txnId == 2)
                event.RequestRetry();
        }));
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 2, 0x78040, false);
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

    REQ creditReturn{};
    creditReturn.SrcID() = 4;
    creditReturn.TgtID() = 3;
    creditReturn.Opcode() = CHI::Eb::Opcodes::REQ::PCrdReturn;
    creditReturn.PCrdType() = 0;
    LUNATIC_CHECK(!driver.node.PushRXREQ(creditReturn));
    driver.Tick();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().returnedCredits == 0);

    creditReturn.TgtID() = 2;
    LUNATIC_CHECK(driver.node.PushRXREQ(creditReturn));
    driver.Tick();
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().returnedCredits == 1);
    LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
}

LUNATIC_TEST(Stage9IngressRejectsMisdirectedCompAck)
{
    TestDriver driver;
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x78080, true)));
    DataBufferId dbid = 0;
    for (size_t beat = 0; beat < 2; ++beat)
        dbid = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).DBID();

    RSP ack{};
    ack.SrcID() = 4;
    ack.TgtID() = 3;
    ack.TxnID() = dbid;
    ack.Opcode() = CHI::Eb::Opcodes::RSP::CompAck;
    LUNATIC_CHECK(!driver.node.PushRXRSP(ack));
    driver.Tick();
    LUNATIC_CHECK(driver.node.HasInflight());

    ack.TgtID() = 2;
    LUNATIC_CHECK(driver.node.PushRXRSP(ack));
    driver.Tick();
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
}

LUNATIC_TEST(Stage9IngressRejectsMisdirectedBackendCreditResponses)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    TestDriver driver(options);
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x780c0, false)));
    const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);

    RSP retry{};
    retry.SrcID() = 6;
    retry.TgtID() = 3;
    retry.TxnID() = downstream.TxnID();
    retry.Opcode() = CHI::Eb::Opcodes::RSP::RetryAck;
    retry.PCrdType() = 3;
    LUNATIC_CHECK(!driver.node.PushRXRSP(retry));
    RSP grant = retry;
    grant.TxnID() = 0;
    grant.Opcode() = CHI::Eb::Opcodes::RSP::PCrdGrant;
    LUNATIC_CHECK(!driver.node.PushRXRSP(grant));

    retry.TgtID() = 2;
    LUNATIC_CHECK(driver.node.PushRXRSP(retry));
    driver.Tick();
    grant.TgtID() = 2;
    LUNATIC_CHECK(driver.node.PushRXRSP(grant));
    driver.Tick();
    const auto resent = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(!resent.AllowRetry() && resent.PCrdType() == 3);
    SendReadData(driver, resent, IngressPattern());
    for (size_t beat = 0; beat < 2; ++beat)
        driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().size() == 2);
}

LUNATIC_TEST(Stage9IngressRejectsInvalidBackendDataRouting)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    TestDriver driver(options);
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x78100, false)));
    const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    const LineData expected = IngressPattern();

    LUNATIC_CHECK(!driver.node.PushRXDAT(MakeReadData(downstream, 3, 2, 0, expected)));
    LUNATIC_CHECK(!driver.node.PushRXDAT(MakeReadData(downstream, 2, 3, 0, expected)));
    SendReadData(driver, downstream, expected);
    for (size_t beat = 0; beat < 2; ++beat)
        driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().size() == 2);
}

LUNATIC_TEST(Stage9IngressRejectsInvalidWriteDataRouting)
{
    TestDriver driver;
    const LineKey line = LineKey::FromAddress(0x78140, false);
    const LineData expected = IngressPattern();
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 1, line.address)));
    const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    const auto makeWrite = [&](NodeId target, NodeId home, size_t beat) {
        return MakeData(4, target, home, dbid, CHI::Eb::Opcodes::DAT::NonCopyBackWrData,
            0, beat, (line.address >> 4) & 3, expected);
    };

    LUNATIC_CHECK(!driver.node.PushRXDAT(makeWrite(3, 0, 0)));
    LUNATIC_CHECK(!driver.node.PushRXDAT(makeWrite(2, 2, 0)));
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(driver.node.PushRXDAT(makeWrite(2, 0, beat)));
    driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().size() == 2);
    LUNATIC_CHECK(driver.node.GetMemory().Read(line) == expected);
}

LUNATIC_TEST(Stage9IngressRejectsMisdirectedSnoopResponse)
{
    TestDriver driver;
    const LineKey line = LineKey::FromAddress(0x78180, false);
    LUNATIC_CHECK(driver.ReadData(6, 90, line.address, CHI::Eb::Opcodes::REQ::ReadShared) == LineData{});
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::MakeUnique, 4, 1, line.address, true)));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(snoop.target == 6);

    RSP response{};
    response.SrcID() = 6;
    response.TgtID() = 3;
    response.TxnID() = snoop.flit.TxnID();
    response.Opcode() = CHI::Eb::Opcodes::RSP::SnpResp;
    response.Resp() = CHI::Eb::Resps::SnpResp_I;
    LUNATIC_CHECK(!driver.node.PushRXRSP(response));
    response.TgtID() = 2;
    LUNATIC_CHECK(driver.node.PushRXRSP(response));
    driver.Tick();
    const auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.CompAck(4, comp.DBID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
}

LUNATIC_TEST(Stage9IngressRejectsInvalidSnoopDataRouting)
{
    TestDriver driver;
    const LineKey line = LineKey::FromAddress(0x781c0, false);
    const LineData expected = IngressPattern();
    SeedDirtyProvider(driver, line, expected);
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 1, line.address, true)));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(snoop.target == 6);
    const auto makeSnoopData = [&](NodeId target, NodeId home, size_t beat) {
        return MakeData(6, target, home, snoop.flit.TxnID(), CHI::Eb::Opcodes::DAT::SnpRespData,
            CHI::Eb::Resps::SnpRespData_I_PD, beat, (line.address >> 4) & 3, expected);
    };

    LUNATIC_CHECK(!driver.node.PushRXDAT(makeSnoopData(3, 0, 0)));
    LUNATIC_CHECK(!driver.node.PushRXDAT(makeSnoopData(2, 2, 0)));
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(driver.node.PushRXDAT(makeSnoopData(2, 0, beat)));
    driver.Tick();
    DataBufferId dbid = 0;
    for (size_t beat = 0; beat < 2; ++beat)
        dbid = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).DBID();
    driver.CompAck(4, dbid);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().size() == 2);
}
