#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData                            CMOData(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 11);
        return data;
    }

    ByteEnables                         CMOEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    void                                CMOWriteback(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId, const LineKey& line,
                                                     const LineData& data, uint8_t resp = CHI::Eb::Resps::CopyBackWrData_UD_PD,
                                                     uint16_t opcode = CHI::Eb::Opcodes::REQ::WriteBackFull)
    {
        driver.PushRequest(driver.Request(opcode, source, txnId, line.address));
        auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(source, txnId, dbid.DBID(), data, CMOEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData, resp);
        driver.Quiesce();
    }

    void                                CMOReceiveUnique(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId)
    {
        DatabaseId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.Resp() == CHI::Eb::Resps::CompData_UC || data.Resp() == CHI::Eb::Resps::CompData_UD_PD);
            dbid = data.DBID();
        }
        driver.CompAck(source, dbid);
    }

    void                                CMOMatrix(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0xc000, false);
        const LineData initial = CMOData(5);
        const LineData dirty = CMOData(83);
        driver.driver.node.GetMemory().WriteFull(line, initial);

        CMOWriteback(driver, 4, 20, line, dirty);
        LUNATIC_CHECK(driver.ReadData(5, 21, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dirty);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::UD);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanShared, 6, 22, line.address));
        auto clean = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanShared);
        LUNATIC_CHECK(clean.target == 5);
        LUNATIC_CHECK(clean.flit.DoNotGoToSD() == 1);
        LUNATIC_CHECK(clean.flit.RetToSrc() == 0);
        driver.SnoopData(5, clean.flit.TxnID(), dirty, CHI::Eb::Resps::SnpRespData_UC_PD);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == dirty);
        LUNATIC_CHECK(!driver.driver.node.InspectHome(line).dirty);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::UC);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanInvalid, 6, 23, line.address));
        auto invalidate = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
        LUNATIC_CHECK(invalidate.flit.DoNotGoToSD() == 1);
        LUNATIC_CHECK(invalidate.flit.RetToSrc() == 0);
        driver.SnoopData(5, invalidate.flit.TxnID(), dirty, CHI::Eb::Resps::SnpRespData_I);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.driver.node.InspectHome(line).valid);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::I);

        LUNATIC_CHECK(driver.ReadData(7, 24, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dirty);
        driver.Quiesce();
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::MakeInvalid, 6, 25, line.address));
        auto make = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpMakeInvalid);
        LUNATIC_CHECK(make.target == 7);
        LUNATIC_CHECK(make.flit.DoNotGoToSD() == 1);
        LUNATIC_CHECK(make.flit.RetToSrc() == 0);
        driver.SnoopResponse(7, make.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.driver.node.InspectHome(line).valid);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 7) == CacheState::I);
    }

    void                                CapacityMatrix(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        options.slcSets = 1;
        options.slcWays = 1;
        options.snoopFilterSets = 8;
        CheckedDriver<Config256> driver(options);
        const LineKey a = LineKey::FromAddress(0xd000, false);
        const LineKey b = LineKey::FromAddress(0xd040, false);
        const LineData dataA = CMOData(17);
        const LineData dataB = CMOData(107);
        driver.driver.node.GetMemory().WriteFull(a, dataA);
        driver.driver.node.GetMemory().WriteFull(b, dataB);

        LUNATIC_CHECK(driver.ReadData(4, 30, a.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dataA);
        driver.Quiesce();
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 31, b.address, true));
        if (policy == PolicyKind::Inclusive)
        {
            auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
            LUNATIC_CHECK(victim.target == 4);
            LUNATIC_CHECK(uint64_t(victim.flit.Addr()) << 3 == a.address);
            driver.SnoopData(4, victim.flit.TxnID(), dataA, CHI::Eb::Resps::SnpRespData_I);
        }
        CMOReceiveUnique(driver, 5, 31);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(b, 5) == CacheState::UC);
        if (policy == PolicyKind::Inclusive)
            LUNATIC_CHECK(driver.driver.node.InspectRequester(a, 4) == CacheState::I);
        else
            LUNATIC_CHECK(driver.driver.node.InspectRequester(a, 4) == CacheState::UC);
        LUNATIC_CHECK(driver.driver.node.InspectHome(b).valid == (policy != PolicyKind::Exclusive));
    }

    void                                DirtyVictimMatrix(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        options.slcSets = 1;
        options.slcWays = 1;
        options.snoopFilterSets = 8;
        CheckedDriver<Config256> driver(options);
        const LineKey a = LineKey::FromAddress(0xe000, false);
        const LineKey b = LineKey::FromAddress(0xe040, false);
        const LineData dataA = CMOData(29);
        const LineData dataB = CMOData(131);
        driver.driver.node.GetMemory().WriteFull(a, CMOData(1));
        driver.driver.node.GetMemory().WriteFull(b, CMOData(2));

        CMOWriteback(driver, 4, 40, a, dataA);
        LUNATIC_CHECK(driver.ReadData(5, 41, a.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dataA);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(a, 5) == CacheState::UD);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 42, b.address));
        auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 42, dbid.DBID(), dataB, CMOEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
                         CHI::Eb::Resps::CopyBackWrData_UD_PD);
        if (policy == PolicyKind::Inclusive)
        {
            auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
            LUNATIC_CHECK(victim.target == 5);
            driver.SnoopData(5, victim.flit.TxnID(), dataA, CHI::Eb::Resps::SnpRespData_I_PD);
        }
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.GetMemory().Read(a) == (policy == PolicyKind::NINE ? CMOData(1) : dataA));
        LUNATIC_CHECK(driver.driver.node.InspectHome(b).data == dataB);
        if (policy == PolicyKind::Inclusive)
            LUNATIC_CHECK(driver.driver.node.InspectRequester(a, 5) == CacheState::I);
        else
            LUNATIC_CHECK(driver.driver.node.InspectRequester(a, 5) == CacheState::UD);
    }

}

LUNATIC_TEST(Stage4CoreCMOsUnderAllPolicies)
{
    CMOMatrix(PolicyKind::NINE);
    CMOMatrix(PolicyKind::Exclusive);
    CMOMatrix(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage4SameLineHazardAndDBIDReuse)
{
    Options options;
    options.memoryLatency = 32;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0xf000, false);
    const LineData data = CMOData(41);
    driver.driver.node.GetMemory().WriteFull(line, data);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 50, line.address, true));
    driver.driver.Tick();
    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 5, 51, line.address, true));
    auto first0 = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    auto first1 = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(first0.TgtID() == 4 && first1.TgtID() == 4);
    LUNATIC_CHECK(!driver.driver.node.PeekTXDAT());
    driver.CompAck(4, first1.DBID());

    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(snoop.target == 4);
    driver.SnoopData(4, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_I);
    CMOReceiveUnique(driver, 5, 51);
    driver.Quiesce();

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 50, line.address, true));
    auto reused = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(reused.target == 5);
    driver.SnoopData(5, reused.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_I);
    CMOReceiveUnique(driver, 4, 50);
    driver.Quiesce();
}

LUNATIC_TEST(Stage4CapacityAndDirtyVictimPolicies)
{
    for (PolicyKind policy : { PolicyKind::NINE, PolicyKind::Exclusive, PolicyKind::Inclusive })
    {
        CapacityMatrix(policy);
        DirtyVictimMatrix(policy);
    }
}

LUNATIC_TEST(Stage4DownstreamCHICMO)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0xf400, false);
    const LineData data = CMOData(57);
    CMOWriteback(driver, 4, 60, line, data);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::MakeInvalid, 4, 61, line.address));
    auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::MakeInvalid);
    LUNATIC_CHECK(downstream.TgtID() == 6);
    Driver<Config256>::RSP response{};
    response.SrcID() = 6;
    response.TgtID() = 2;
    response.TxnID() = downstream.TxnID();
    response.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
    driver.driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.Quiesce();
}
