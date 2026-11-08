#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData RemedData(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 3);
        return data;
    }

    ByteEnables RemedEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    void RemedWriteback(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId,
                                                       const LineKey& line, const LineData& data, uint8_t resp = CHI::Eb::Resps::CopyBackWrData_UD_PD,
                                                       uint16_t opcode = CHI::Eb::Opcodes::REQ::WriteBackFull, bool partial = false)
    {
        if (partial)
            driver.driver.node.GetMemory().WriteFull(line, data);
        const uint16_t requestOpcode = partial ? uint16_t(CHI::Eb::Opcodes::REQ::WriteBackPtl) : uint16_t(opcode);
        driver.PushRequest(driver.Request(requestOpcode, source, txnId, line.address));
        auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        ByteEnables enables = RemedEnables();
        if (partial)
            for (size_t index = 0; index < enables.size(); ++index)
                enables[index] = index % 32 < 16;
        driver.WriteData(source, txnId, dbid.DBID(), data, enables, CHI::Eb::Opcodes::DAT::CopyBackWrData, resp);
        driver.Quiesce();
    }

    LineData RemedReceive(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId,
                                                     uint8_t resp, bool acknowledge = false)
    {
        LineData result{};
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.Resp() == resp);
            dbid = data.DBID();
            const size_t offset = size_t(data.DataID()) * 16;
            for (size_t index = 0; index < 32; ++index)
                result[offset + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
        }
        if (acknowledge)
            driver.CompAck(source, dbid);
        return result;
    }

    void ReadOnceDirtyPolicy(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        options.slcSets = 1;
        options.slcWays = 1;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x15000, false);
        const LineKey other = LineKey::FromAddress(0x15040, false);
        const LineData data = RemedData(71);
        RemedWriteback(driver, 4, 91, line, data);
        LUNATIC_CHECK(driver.ReadData(5, 92, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        driver.Quiesce();

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadOnce, 6, 93, line.address));
        auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpOnce);
        driver.SnoopData(5, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_I_PD);
        LUNATIC_CHECK(RemedReceive(driver, 6, 93, CHI::Eb::Resps::CompData_UC) == data);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).dirty);

        RemedWriteback(driver, 7, 94, other, RemedData(99));
        LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == data);
    }

    template<typename config>
    void SublineReadGeometry()
    {
        Driver<config> driver;
        const LineKey line = LineKey::FromAddress(0x16000, false);
        const LineData data = RemedData(103);
        driver.node.GetMemory().WriteFull(line, data);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 100, line.address + 0x30);
        request.Size() = 4;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        auto response = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(response.DataID() == (config::dataWidth == 512 ? 0 : 3));
        LUNATIC_CHECK(response.CCID() == 3);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.node.PeekTXDAT());
    }

}

LUNATIC_TEST(Stage4ReadOncePassDirtySurvivesVictim)
{
    ReadOnceDirtyPolicy(PolicyKind::NINE);
    ReadOnceDirtyPolicy(PolicyKind::Exclusive);
    ReadOnceDirtyPolicy(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage4SilentUniqueTransitionsReadOnceAndCleanShared)
{
    for (const bool dirtyProvider : { false, true })
    {
        Options options;
        options.policy = PolicyKind::NINE;
        options.slcSets = 1;
        options.slcWays = 1;
        options.preciseSnoopFilter = true;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x15100, false);
        const LineData oldData = RemedData(41);
        const LineData newData = RemedData(173);
        driver.driver.node.GetMemory().WriteFull(line, oldData);
        LUNATIC_CHECK(driver.ReadData(5, 92, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == oldData);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).valid);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadOnce, 6, 93, line.address));
        const auto once = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpOnce);
        LUNATIC_CHECK(once.target == 5);
        driver.SnoopData(5, once.flit.TxnID(), dirtyProvider ? newData : oldData,
            dirtyProvider ? CHI::Eb::Resps::SnpRespData_UD : CHI::Eb::Resps::SnpRespData_UC);
        LUNATIC_CHECK(RemedReceive(driver, 6, 93, CHI::Eb::Resps::CompData_UC) ==
            (dirtyProvider ? newData : oldData));
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::UD);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanShared, 6, 94, line.address));
        const auto clean = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanShared);
        LUNATIC_CHECK(clean.target == 5);
        driver.SnoopData(5, clean.flit.TxnID(), dirtyProvider ? newData : oldData,
            CHI::Eb::Resps::SnpRespData_UC_PD);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == (dirtyProvider ? newData : oldData));
    }
}

LUNATIC_TEST(Stage4WriteCleanFullSDPDBecomesSC)
{
    for (PolicyKind policy : { PolicyKind::NINE, PolicyKind::Exclusive, PolicyKind::Inclusive })
    {
        Options options;
        options.policy = policy;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x17000, false);
        const LineData data = RemedData(107);
        RemedWriteback(driver, 4, 110, line, data);
        LUNATIC_CHECK(driver.ReadData(5, 111, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        driver.Quiesce();
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 6, 112, line.address, true));
        auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
        driver.SnoopData(5, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_SC_PD);
        LUNATIC_CHECK(RemedReceive(driver, 6, 112, CHI::Eb::Resps::CompData_SD_PD, true) == data);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::SD);

        RemedWriteback(driver, 6, 113, line, data, CHI::Eb::Resps::CopyBackWrData_SD_PD,
                       CHI::Eb::Opcodes::REQ::WriteCleanFull);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::SC);
    }
}

LUNATIC_TEST(Stage4SingleWriteDataCancelCompletes)
{
    CheckedDriver<Config256> driver;
    const LineKey line = LineKey::FromAddress(0x18000, false);
    const LineData original = RemedData(113);
    driver.driver.node.GetMemory().WriteFull(line, original);
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl, 4, 120, line.address));
    auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    Driver<Config256>::DAT cancel{};
    cancel.SrcID() = 4;
    cancel.TgtID() = 2;
    cancel.TxnID() = dbid.DBID();
    cancel.DBID() = dbid.DBID();
    cancel.Opcode() = CHI::Eb::Opcodes::DAT::WriteDataCancel;
    for (size_t dataId : { size_t(0), size_t(2) })
    {
        cancel.DataID() = dataId;
        driver.Oracle(4).WriteData(driver.driver.now, cancel);
        LUNATIC_CHECK(driver.driver.node.PushRXDAT(cancel));
    }
    driver.driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == original);
}

LUNATIC_TEST(Stage4SublineReadAndCCID128)
{
    SublineReadGeometry<Config128>();
}

LUNATIC_TEST(Stage4SublineReadAndCCID512)
{
    SublineReadGeometry<Config512>();
}

LUNATIC_TEST(Stage4SublineCHIBackendRead)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    Driver<Config128> driver(options);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 121, 0x16030);
    request.Size() = 4;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(downstream.Size() == 4);
    Driver<Config128>::DAT data{};
    data.SrcID() = 6;
    data.TgtID() = 2;
    data.HomeNID() = 2;
    data.TxnID() = downstream.TxnID();
    data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    data.DataID() = 3;
    data.CCID() = 3;
    LUNATIC_CHECK(driver.node.PushRXDAT(data));
    driver.Tick();
    auto response = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(response.DataID() == 3 && response.CCID() == 3);
    driver.Quiesce();
}

LUNATIC_TEST(Stage4ReadReceiptPrecedesDataEmission)
{
    Driver<Config256> driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 122, 0x19000);
    request.Order() = 2;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    driver.Until([&] { return static_cast<bool>(driver.node.PeekTXRSP()); });
    for (size_t cycle = 0; cycle < 20; ++cycle)
    {
        driver.Tick();
        LUNATIC_CHECK(!driver.node.PeekTXDAT());
    }
    auto receipt = driver.node.PopTXRSP();
    LUNATIC_CHECK(receipt && receipt->Opcode() == CHI::Eb::Opcodes::RSP::ReadReceipt);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.Quiesce();
}

LUNATIC_TEST(Stage4EarlyCompAckDoesNotRetire)
{
    Driver<Config256> driver;
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 123, 0x1a000, true)));
    driver.Until([&] { return driver.node.GetTime() >= 4; });
    Driver<Config256>::RSP ack{};
    ack.SrcID() = 4;
    ack.TgtID() = 2;
    ack.TxnID() = 0;
    ack.Opcode() = CHI::Eb::Opcodes::RSP::CompAck;
    LUNATIC_CHECK(driver.node.PushRXRSP(ack));
    driver.Tick();
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    auto first = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.CompAck(4, first.DBID());
    driver.Quiesce();
}

LUNATIC_TEST(Stage4UnsupportedRequestGetsErrorCompletion)
{
    Driver<Config256> driver;
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::AtomicSwap, 4, 124, 0x1b000)));
    auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(response.RespErr() == 0b10);
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    driver.Quiesce();
}

LUNATIC_TEST(Stage4SublineWriteDataCancelCompletes)
{
    CheckedDriver<Config256> driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl, 4, 130, 0x29020);
    request.Size() = 5;
    driver.PushRequest(request);
    auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    Driver<Config256>::DAT cancel{};
    cancel.SrcID() = 4;
    cancel.TgtID() = 2;
    cancel.TxnID() = dbid.DBID();
    cancel.DBID() = 0;
    cancel.Opcode() = CHI::Eb::Opcodes::DAT::WriteDataCancel;
    cancel.DataID() = 2;
    cancel.CCID() = 2;
    driver.Oracle(4).WriteData(driver.driver.now, cancel);
    LUNATIC_CHECK(driver.driver.node.PushRXDAT(cancel));
    driver.driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
}

LUNATIC_TEST(Stage4InternalBaselinePreservesPartialSnoopBytes)
{
    Options options;
    options.policy = PolicyKind::Exclusive;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x2b000, false);
    const LineData newer = RemedData(41);
    const LineData baseline = RemedData(201);
    RemedWriteback(driver, 4, 131, line, newer, CHI::Eb::Resps::CopyBackWrData_UD_PD,
        CHI::Eb::Opcodes::REQ::WriteBackFull, true);
    LUNATIC_CHECK(driver.ReadData(5, 132, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == newer);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::UDP);
    driver.driver.node.GetMemory().WriteFull(line, baseline);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 133, line.address, true));
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    driver.SnoopData(5, snoop.flit.TxnID(), newer, CHI::Eb::Resps::SnpRespDataPtl_I_PD, true);
    LineData expected = baseline;
    for (size_t index = 0; index < expected.size(); ++index)
        if (index % 32 < 16)
            expected[index] = newer[index];
    LUNATIC_CHECK(RemedReceive(driver, 6, 133, CHI::Eb::Resps::CompData_UD_PD, true) == expected);
    driver.Quiesce();
}

LUNATIC_TEST(Stage4NormalUnalignedWindowRead128)
{
    Driver<Config128> driver;
    const LineKey line = LineKey::FromAddress(0x78, false);
    const LineData expected = RemedData(173);
    driver.node.GetMemory().WriteFull(line, expected);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 140, line.address + 0x38);
    request.Size() = 5;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));

    bool seen[4]{};
    for (size_t packet = 0; packet < 2; ++packet)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.CCID() == 3);
        seen[size_t(data.DataID())] = true;
        const size_t offset = size_t(data.DataID()) * 16;
        for (size_t index = 0; index < 16; ++index)
            LUNATIC_CHECK(uint8_t(data.Data()[index / 8] >> ((index % 8) * 8)) == expected[offset + index]);
    }
    driver.Quiesce();
    LUNATIC_CHECK(seen[2] && seen[3]);
    LUNATIC_CHECK(!driver.node.PeekTXDAT());
}

LUNATIC_TEST(Stage4NormalUnalignedWindowWrite128)
{
    CheckedDriver<Config128> driver;
    const LineKey line = LineKey::FromAddress(0x78, false);
    const LineData initial = RemedData(179);
    const LineData update = RemedData(211);
    LineData payload{};
    ByteEnables enables{};
    LineData expected = initial;
    driver.driver.node.GetMemory().WriteFull(line, initial);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl, 4, 141, line.address + 0x38);
    request.Size() = 5;
    driver.PushRequest(request);
    const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    for (size_t offset = 0x20; offset < 0x40; ++offset)
        if (offset < 0x28 || offset >= 0x38)
        {
            enables[offset] = 1;
            payload[offset] = update[offset];
            expected[offset] = update[offset];
        }
    driver.WriteData(4, 141, dbid, payload, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData, 0, 0b1100, 3);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == expected);
}

LUNATIC_TEST(Stage4DeviceUnalignedReadIncludesRedundantPacket)
{
    Driver<Config128> driver;
    const LineKey line = LineKey::FromAddress(0x58, false);
    const LineData expected = RemedData(227);
    driver.node.GetMemory().WriteFull(line, expected);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 142, line.address + 0x18);
    request.Size() = 6;
    request.MemAttr() = CHI::Eb::MemAttrs::Device_nEWA;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));

    bool seen[4]{};
    for (size_t packet = 0; packet < 4; ++packet)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        const size_t offset = size_t(data.DataID()) * 16;
        seen[size_t(data.DataID())] = true;
        LUNATIC_CHECK(data.CCID() == 1);
        for (size_t index = 0; index < 16; ++index)
        {
            const bool valid = offset + index >= 0x18;
            LUNATIC_CHECK(bool((uint64_t(data.BE()) >> index) & 1) == valid);
            const uint8_t value = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
            LUNATIC_CHECK(value == (valid ? expected[offset + index] : 0));
        }
    }
    driver.Quiesce();
    for (bool present : seen)
        LUNATIC_CHECK(present);
    LUNATIC_CHECK(!driver.node.PeekTXDAT());
}

LUNATIC_TEST(Stage4TransferGeometryExamples)
{
    for (size_t width : { size_t(128), size_t(256), size_t(512) })
    {
        Options options;
        options.dataWidth = width;
        Request normal{};
        normal.address = 0x78;
        normal.size = 5;
        normal.memAttr = CHI::Eb::MemAttrs::WriteBack_Allocate;
        const TransferGeometry normalGeometry = GetTransferGeometry(normal, options);
        LUNATIC_CHECK(normalGeometry.alignedAddress == 0x60);
        LUNATIC_CHECK(normalGeometry.windowOffset == 0x20);
        LUNATIC_CHECK(normalGeometry.rawOffset == 0x38);
        LUNATIC_CHECK(normalGeometry.transferBytes == 32);
        LUNATIC_CHECK(normalGeometry.validOffset == 0x20);
        LUNATIC_CHECK(normalGeometry.validBytes == 32);
        LUNATIC_CHECK(normalGeometry.ccid == 3);
        LUNATIC_CHECK(normalGeometry.beatMask ==
            (width == 128 ? uint64_t(0b1100) : width == 256 ? uint64_t(0b0010) : uint64_t(0b0001)));

        Request device = normal;
        device.address = 0x58;
        device.size = 6;
        device.memAttr = CHI::Eb::MemAttrs::Device_nEWA;
        const TransferGeometry deviceGeometry = GetTransferGeometry(device, options);
        LUNATIC_CHECK(deviceGeometry.alignedAddress == 0x40);
        LUNATIC_CHECK(deviceGeometry.windowOffset == 0);
        LUNATIC_CHECK(deviceGeometry.rawOffset == 0x18);
        LUNATIC_CHECK(deviceGeometry.transferBytes == 64);
        LUNATIC_CHECK(deviceGeometry.validOffset == 0x18);
        LUNATIC_CHECK(deviceGeometry.validBytes == 40);
        LUNATIC_CHECK(deviceGeometry.ccid == 1);
        LUNATIC_CHECK(deviceGeometry.beatMask ==
            (width == 128 ? uint64_t(0b1111) : width == 256 ? uint64_t(0b0011) : uint64_t(0b0001)));

        Request invalid = normal;
        invalid.size = 7;
        const TransferGeometry invalidGeometry = GetTransferGeometry(invalid, options);
        LUNATIC_CHECK(invalidGeometry.transferBytes == 0);
        LUNATIC_CHECK(invalidGeometry.beatMask == 0);
    }
}

LUNATIC_TEST(Stage4MultiPacketWriteDataCancelCompletes)
{
    const auto run = [](bool reverse) {
        CheckedDriver<Config128> driver;
        const LineKey line = LineKey::FromAddress(0x78, false);
        const LineData initial = RemedData(239);
        driver.driver.node.GetMemory().WriteFull(line, initial);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl, 4, 150, line.address + 0x38);
        request.Size() = 5;
        driver.PushRequest(request);
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        const auto cancel = [&](size_t dataId) {
            Driver<Config128>::DAT data{};
            data.SrcID() = 4;
            data.TgtID() = 2;
            data.TxnID() = dbid;
            data.Opcode() = CHI::Eb::Opcodes::DAT::WriteDataCancel;
            data.Resp() = 0;
            data.DataID() = dataId;
            data.CCID() = 3;
            driver.Oracle(4).WriteData(driver.driver.now, data);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
            driver.driver.Tick();
        };

        cancel(reverse ? 3 : 2);
        for (size_t cycle = 0; cycle < 4; ++cycle)
        {
            driver.driver.Tick();
            LUNATIC_CHECK(!driver.driver.node.PeekTXRSP());
        }
        cancel(reverse ? 2 : 3);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == initial);
    };
    run(false);
    run(true);
}
