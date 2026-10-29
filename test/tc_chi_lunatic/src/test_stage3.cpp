#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData Pattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 13);
        return data;
    }

    template<typename config>
    void ReadGeometry()
    {
        Driver<config> driver;
        const LineKey line = LineKey::FromAddress(0x2000, false);
        const LineData expected = Pattern(7);
        driver.node.GetMemory().WriteFull(line, expected);
        LUNATIC_CHECK(driver.ReadData(4, 1, line.address, CHI::Eb::Opcodes::REQ::ReadNoSnp) == expected);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(4, 2, line.address, CHI::Eb::Opcodes::REQ::ReadOnce, true) == expected);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    template<typename config>
    void SendBackendReadData(Driver<config>& driver, TransactionId txnId, const LineData& line)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            typename Driver<config>::DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 2;
            data.HomeNID() = 2;
            data.TxnID() = txnId;
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = CHI::Eb::Resps::CompData_I;
            data.DataID() = beat * bytes / 16;
            data.CCID() = (beat * bytes / 16) % 2;
            for (size_t index = 0; index < bytes; ++index)
                data.Data()[index / 8] |= uint64_t(line[beat * bytes + index]) << ((index % 8) * 8);
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
    }

}

LUNATIC_TEST(Stage3ReadNoSnpAndReadOnce128)
{
    ReadGeometry<Config128>();
}

LUNATIC_TEST(Stage3ReadNoSnpAndReadOnce256)
{
    ReadGeometry<Config256>();
}

LUNATIC_TEST(Stage3ReadNoSnpAndReadOnce512)
{
    ReadGeometry<Config512>();
}

LUNATIC_TEST(Stage3WriteNoSnpFullPartialAndZero)
{
    Driver<Config256> driver;
    const LineKey line = LineKey::FromAddress(0x3000, false);
    const LineData original = Pattern(11);
    const LineData update = Pattern(91);
    driver.node.GetMemory().WriteFull(line, original);

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 20, line.address)));
    auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    ByteEnables full{};
    full.fill(1);
    driver.WriteData(4, 20, dbid.DBID(), update, full, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetMemory().Read(line) == update);

    LineData partial = Pattern(151);
    ByteEnables enables{};
    for (size_t index = 5; index < 13; ++index)
        enables[index] = 1;
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl, 4, 21, line.address)));
    dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    driver.WriteData(4, 21, dbid.DBID(), partial, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LineData merged = update;
    for (size_t index = 5; index < 13; ++index)
        merged[index] = partial[index];
    LUNATIC_CHECK(driver.node.GetMemory().Read(line) == merged);

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpZero, 4, 22, line.address)));
    driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetMemory().Read(line) == LineData{});
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage3OutputBackpressureAndPurePeek)
{
    Options options;
    options.outputQueueLimit = 1;
    Driver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x4000, false);
    driver.node.GetMemory().WriteFull(line, Pattern(3));
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 30, line.address)));
    driver.Until([&] { return static_cast<bool>(driver.node.PeekTXDAT()); });
    const size_t pending = driver.node.GetEvents()->Pending();
    const auto first = driver.node.PeekTXDAT();
    for (size_t index = 0; index < 5; ++index)
    {
        const auto next = driver.node.PeekTXDAT();
        LUNATIC_CHECK(next && first);
        LUNATIC_CHECK(next->DataID() == first->DataID());
        LUNATIC_CHECK(next->TxnID() == first->TxnID());
        LUNATIC_CHECK(driver.node.GetEvents()->Pending() == pending);
    }
    auto beat0 = driver.node.PopTXDAT();
    LUNATIC_CHECK(beat0 && beat0->DataID() == 0);
    auto beat1 = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(beat1.DataID() == 2);
    driver.Quiesce();
}

LUNATIC_TEST(Stage3CHIBackendRead)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    Driver<Config256> driver(options);
    const LineData expected = Pattern(23);
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 40, 0x5000)));
    auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(downstream.SrcID() == 2);
    LUNATIC_CHECK(downstream.TgtID() == 6);
    SendBackendReadData(driver, downstream.TxnID(), expected);
    auto beat0 = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    auto beat1 = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(beat0.TgtID() == 4 && beat1.TgtID() == 4);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage3CHIBackendWriteSeparatedResponses)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    Driver<Config256> driver(options);
    const LineData expected = Pattern(37);
    ByteEnables full{};
    full.fill(1);

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 41, 0x6000)));
    auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    driver.WriteData(4, 41, dbid.DBID(), expected, full, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);

    Driver<Config256>::RSP response{};
    response.SrcID() = 6;
    response.TgtID() = 2;
    response.TxnID() = downstream.TxnID();
    response.DBID() = 77;
    response.Opcode() = CHI::Eb::Opcodes::RSP::DBIDResp;
    LUNATIC_CHECK(driver.node.PushRXRSP(response));
    driver.Tick();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    auto beat0 = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    auto beat1 = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    LUNATIC_CHECK(beat0.TgtID() == 6 && beat0.DBID() == 0);
    LUNATIC_CHECK(beat1.TgtID() == 6 && beat1.DBID() == 0);

    response.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
    LUNATIC_CHECK(driver.node.PushRXRSP(response));
    driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage3BasicRetryGrantAndResend)
{
    Options options;
    options.transactionLimit = 1;
    Driver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x7000, false);
    driver.node.GetMemory().WriteFull(line, Pattern(49));
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 50, line.address)));
    driver.Tick();
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 51, line.address)));
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);
    driver.Quiesce();

    auto retry = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 51, line.address);
    retry.AllowRetry() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(retry));
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage3UnmatchedResponseIsReported)
{
    Driver<Config256> driver;
    Driver<Config256>::RSP response{};
    response.SrcID() = 4;
    response.TgtID() = 2;
    response.TxnID() = 999;
    response.Opcode() = CHI::Eb::Opcodes::RSP::CompAck;
    LUNATIC_CHECK(driver.node.PushRXRSP(response));
    driver.Tick();
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    driver.Quiesce();
}

LUNATIC_TEST(Stage3BridgeReadFlow)
{
    Bridge<Config256> bridge(2);
    const LineKey line = LineKey::FromAddress(0x8000, false);
    const LineData expected = Pattern(61);
    bridge.GetNode().GetMemory().WriteFull(line, expected);
    HomeNode<Config256>::REQFlit request{};
    request.SrcID() = 4;
    request.TgtID() = 2;
    request.TxnID() = 60;
    request.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    request.Addr() = line.address;
    request.Size() = 6;
    LUNATIC_CHECK(bridge.PushUpstreamREQ(request));
    LineData actual{};
    uint64_t time = 0;
    for (size_t beat = 0; beat < 2; ++beat)
    {
        while (!bridge.PeekUpstreamDAT())
            bridge.Tick(++time);
        auto data = bridge.PopUpstreamDAT();
        LUNATIC_CHECK(data);
        for (size_t index = 0; index < 32; ++index)
            actual[beat * 32 + index] = uint8_t(data->Data()[index / 8] >> ((index % 8) * 8));
    }
    while (!bridge.Drain())
        bridge.Tick(++time);
    LUNATIC_CHECK(actual == expected);
}
