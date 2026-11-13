#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = CheckedDriver<Config256>;

    ByteEnables SnoopIdEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    LineData SnoopIdPattern()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(0x93 + index * 11);
        return data;
    }

    void SeedDirtyProvider(TestDriver& driver, const LineKey& line, const LineData& data)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 9, 90, line.address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(9, 90, response.DBID(), data, SnoopIdEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
            CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(9, 91, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 9) == CacheState::UD);
    }

}

LUNATIC_TEST(Stage9SnoopIdGlobalUniquenessAndDCTAck)
{
    Options options;
    options.transactionLimit = 2;
    options.snoopLimit = 2;
    options.dctEnabled = true;
    TestDriver driver(options);
    const LineKey ordinaryLine = LineKey::FromAddress(0x7a000, false);
    const LineKey forwardingLine = LineKey::FromAddress(0x7a040, false);
    const LineData expected = SnoopIdPattern();

    LUNATIC_CHECK(driver.ReadData(6, 60, ordinaryLine.address, CHI::Eb::Opcodes::REQ::ReadShared) == LineData{});
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 8, 61, ordinaryLine.address, true));
    const auto seedSnoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
    LUNATIC_CHECK(seedSnoop.target == 6);
    driver.SnoopData(6, seedSnoop.flit.TxnID(), LineData{}, CHI::Eb::Resps::SnpRespData_SC);
    DataBufferId seedDBID = 0;
    for (size_t beat = 0; beat < 2; ++beat)
        seedDBID = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).DBID();
    driver.CompAck(8, seedDBID);
    driver.Quiesce();
    SeedDirtyProvider(driver, forwardingLine, expected);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::MakeUnique, 5, 10, ordinaryLine.address, true));
    const auto first = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    const auto second = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(first.target == 6 && second.target == 8);
    LUNATIC_CHECK(first.flit.TxnID() != second.flit.TxnID());

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 11, forwardingLine.address, true);
    request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    driver.PushRequest(request);
    const auto forwarding = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpSharedFwd);
    LUNATIC_CHECK(forwarding.target == 9);
    driver.Oracle(9).RegisterRequester(4);
    LUNATIC_CHECK(forwarding.flit.FwdNID() == 4 && forwarding.flit.FwdTxnID() == 11);
    LUNATIC_CHECK(forwarding.flit.TxnID() != first.flit.TxnID());
    LUNATIC_CHECK(forwarding.flit.TxnID() != second.flit.TxnID());

    for (size_t beat = 0; beat < 2; ++beat)
    {
        TestDriver::DAT data{};
        data.SrcID() = 9;
        data.TgtID() = 4;
        data.HomeNID() = 2;
        data.TxnID() = 11;
        data.DBID() = forwarding.flit.TxnID();
        data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data.Resp() = CHI::Eb::Resps::CompData_SD_PD;
        data.DataID() = beat * 2;
        data.CCID() = 0;
        data.BE() = ~uint64_t(0);
        for (size_t index = 0; index < Config256::dataWidth / 8; ++index)
            data.Data()[index / 8] |= uint64_t(expected[beat * (Config256::dataWidth / 8) + index]) << ((index % 8) * 8);
        driver.Oracle(9).SnoopData(driver.driver.now, data);
        driver.Oracle(4).Data(driver.driver.now, data);
    }

    TestDriver::RSP forwarded{};
    forwarded.SrcID() = 9;
    forwarded.TgtID() = 2;
    forwarded.TxnID() = forwarding.flit.TxnID();
    forwarded.Opcode() = CHI::Eb::Opcodes::RSP::SnpRespFwded;
    forwarded.Resp() = CHI::Eb::Resps::SnpResp_I;
    forwarded.FwdState() = CHI::Eb::Resps::CompData_SD_PD;
    driver.Oracle(9).SnoopResponse(driver.driver.now, forwarded);
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(forwarded));
    driver.driver.Tick();
    LUNATIC_CHECK(!driver.driver.node.PeekTXDAT());
    driver.CompAck(4, forwarding.flit.TxnID());

    driver.SnoopResponse(8, second.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    driver.SnoopResponse(6, first.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    const auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(comp.TgtID() == 5 && comp.TxnID() == 10);
    driver.CompAck(5, comp.DBID());
    driver.Quiesce();
}

LUNATIC_TEST(Stage9SnoopIdAllocatorRolloverAndExhaustion)
{
    SnoopIdAllocator allocator;
    std::set<TransactionId> allocated;
    for (size_t index = 0; index < 4096; ++index)
    {
        const auto id = allocator.Allocate();
        LUNATIC_CHECK(id);
        LUNATIC_CHECK(allocated.insert(*id).second);
    }
    LUNATIC_CHECK(allocator.Size() == 4096);
    LUNATIC_CHECK(!allocator.Allocate());

    LUNATIC_CHECK(allocator.Release(17));
    const auto recycled = allocator.Allocate();
    LUNATIC_CHECK(recycled && *recycled == 17);
    allocator.Clear();
    LUNATIC_CHECK(allocator.Size() == 0);

    const auto filtered = allocator.Allocate([](TransactionId id) { return id != 0; });
    LUNATIC_CHECK(filtered && *filtered == 1);
    LUNATIC_CHECK(allocator.Release(1));
    const auto reused = allocator.Allocate();
    LUNATIC_CHECK(reused && *reused == 2);
}

LUNATIC_TEST(Stage9SnoopIdDCTRebindAllowsSubsequentRequesterDBID)
{
    Options options;
    options.transactionLimit = 2;
    options.dctEnabled = true;
    TestDriver driver(options);
    const LineKey forwardingLine = LineKey::FromAddress(0x7a100, false);
    const LineData expected = SnoopIdPattern();
    SeedDirtyProvider(driver, forwardingLine, expected);

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 11, forwardingLine.address, true);
    request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    driver.PushRequest(request);
    const auto forwarding = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpSharedFwd);
    LUNATIC_CHECK(forwarding.flit.TxnID() == 1);
    driver.Oracle(9).RegisterRequester(4);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 12, 0x7a140, false));
    for (size_t cycle = 0; cycle < 12; ++cycle)
        driver.driver.Tick();
    LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.TgtID() == 4 && data.TxnID() == 12);
    }

    for (size_t beat = 0; beat < 2; ++beat)
    {
        TestDriver::DAT data{};
        data.SrcID() = 9;
        data.TgtID() = 4;
        data.HomeNID() = 2;
        data.TxnID() = 11;
        data.DBID() = forwarding.flit.TxnID();
        data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data.Resp() = CHI::Eb::Resps::CompData_SD_PD;
        data.DataID() = beat * 2;
        data.CCID() = 0;
        data.BE() = ~uint64_t(0);
        for (size_t index = 0; index < Config256::dataWidth / 8; ++index)
            data.Data()[index / 8] |= uint64_t(expected[beat * (Config256::dataWidth / 8) + index]) << ((index % 8) * 8);
        driver.Oracle(9).SnoopData(driver.driver.now, data);
        driver.Oracle(4).Data(driver.driver.now, data);
    }

    TestDriver::RSP forwarded{};
    forwarded.SrcID() = 9;
    forwarded.TgtID() = 2;
    forwarded.TxnID() = forwarding.flit.TxnID();
    forwarded.Opcode() = CHI::Eb::Opcodes::RSP::SnpRespFwded;
    forwarded.Resp() = CHI::Eb::Resps::SnpResp_I;
    forwarded.FwdState() = CHI::Eb::Resps::CompData_SD_PD;
    driver.Oracle(9).SnoopResponse(driver.driver.now, forwarded);
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(forwarded));
    driver.driver.Tick();
    driver.CompAck(4, forwarding.flit.TxnID());
    driver.Quiesce();
}
