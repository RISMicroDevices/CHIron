#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData PersistencePattern()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(43 + index * 5);
        return data;
    }

    ByteEnables PersistenceEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    template<typename config>
    void SeedDirtyHome(CheckedDriver<config>& driver, const LineKey& line, const LineData& data)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 1, line.address));
        auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 1, response.DBID(), data, PersistenceEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
    }

    template<typename config>
    LineData ReceiveCompData(CheckedDriver<config>& driver, NodeId source, TransactionId txnId)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        LineData result{};
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TgtID() == source && data.TxnID() == txnId);
            dbid = data.DBID();
            const size_t offset = size_t(data.DataID()) * 16;
            for (size_t index = 0; index < bytes; ++index)
                result[offset + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
        }
        driver.CompAck(source, dbid);
        return result;
    }

    template<typename config>
    void CompleteCleanUnique(CheckedDriver<config>& driver, const LineKey& line, TransactionId txnId,
                             CacheState expected, bool cleanPeer = false)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanUnique, 5, txnId, line.address, true));
        if (cleanPeer)
        {
            auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
            LUNATIC_CHECK(snoop.target == 6);
            LUNATIC_CHECK(snoop.flit.RetToSrc() == 0);
            driver.SnoopResponse(6, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        }
        auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(comp.TxnID() == txnId);
        LUNATIC_CHECK(comp.Resp() == CHI::Eb::Resps::Comp_UC);
        driver.CompAck(5, comp.DBID());
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == expected);
    }

    void RunCleanUniqueState(PolicyKind policy, CacheState initial)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x4a000, false);
        const LineData data = PersistencePattern();
        driver.driver.node.GetMemory().WriteFull(line, data);
        TransactionId txnId = 10;
        bool cleanPeer = false;

        switch (initial)
        {
            case CacheState::I:
            case CacheState::UCE:
                break;
            case CacheState::UC:
                LUNATIC_CHECK(driver.ReadData(5, txnId++, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
                break;
            case CacheState::SC:
                LUNATIC_CHECK(driver.ReadData(5, txnId++, line.address, CHI::Eb::Opcodes::REQ::ReadShared) == data);
                break;
            case CacheState::SD:
            {
                SeedDirtyHome(driver, line, data);
                LUNATIC_CHECK(driver.ReadData(6, txnId++, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
                driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 5, txnId++, line.address, true));
                auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
                driver.SnoopData(6, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_UC_PD);
                LUNATIC_CHECK(ReceiveCompData(driver, 5, txnId - 1) == data);
                cleanPeer = true;
                break;
            }
            case CacheState::UD:
                SeedDirtyHome(driver, line, data);
                LUNATIC_CHECK(driver.ReadData(5, txnId++, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
                break;
            default:
                LUNATIC_CHECK(false);
        }
        if (initial != CacheState::I && initial != CacheState::UCE)
            LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == initial);

        const CacheState expected = initial == CacheState::I || initial == CacheState::UCE ? CacheState::UCE
            : initial == CacheState::SD || initial == CacheState::UD ? CacheState::UD : CacheState::UC;
        if (initial == CacheState::UCE)
            CompleteCleanUnique(driver, line, txnId++, CacheState::UCE);
        CompleteCleanUnique(driver, line, txnId, expected, cleanPeer);
    }

    void RunCleanUniquePeer(PolicyKind policy, bool dirty)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(dirty ? 0x4a100 : 0x4a200, false);
        const LineData data = PersistencePattern();
        driver.driver.node.GetMemory().WriteFull(line, data);
        if (dirty)
        {
            SeedDirtyHome(driver, line, data);
            LUNATIC_CHECK(driver.ReadData(6, 2, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
            LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UD);
        }
        else
            LUNATIC_CHECK(driver.ReadData(6, 2, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanUnique, 5, 3, line.address, true));
        auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
        LUNATIC_CHECK(snoop.target == 6 && snoop.flit.RetToSrc() == 0);
        if (dirty)
            driver.SnoopData(6, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_I_PD);
        else
            driver.SnoopResponse(6, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(comp.Resp() == CHI::Eb::Resps::Comp_UC);
        driver.CompAck(5, comp.DBID());
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::UCE);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::I);
        LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == data);
        LUNATIC_CHECK(!driver.driver.node.InspectHome(line).valid || !driver.driver.node.InspectHome(line).dirty);
    }

}

LUNATIC_TEST(Stage5CleanUniqueRequesterStates)
{
    const CacheState states[] = {
        CacheState::I, CacheState::UCE, CacheState::UC, CacheState::SC, CacheState::SD, CacheState::UD
    };
    const PolicyKind policies[] = { PolicyKind::NINE, PolicyKind::Exclusive, PolicyKind::Inclusive };
    for (PolicyKind policy : policies)
        for (CacheState state : states)
            RunCleanUniqueState(policy, state);
}

LUNATIC_TEST(Stage5CleanUniqueCleanAndDirtyPeers)
{
    const PolicyKind policies[] = { PolicyKind::NINE, PolicyKind::Exclusive, PolicyKind::Inclusive };
    for (PolicyKind policy : policies)
    {
        RunCleanUniquePeer(policy, false);
        RunCleanUniquePeer(policy, true);
    }
}

LUNATIC_TEST(Stage5CleanUniqueRejectsInvalidRequests)
{
    {
        Driver<Config256> driver;
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanUnique, 5, 1, 0x4a300, true);
        request.Size() = 4;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(comp.RespErr() == 0b10);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
    }
    {
        Driver<Config256> driver;
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanUnique, 5, 2, 0x4a300, false);
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(comp.RespErr() == 0b10);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
    }
}

LUNATIC_TEST(Stage5CleanUniqueRejectsEarlyCompAck)
{
    Driver<Config256> driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanUnique, 5, 3, 0x4a400, true);
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    driver.Until([&] { return static_cast<bool>(driver.node.PeekTXRSP()); });
    driver.CompAck(5, 0);
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.CompAck(5, comp.DBID());
    driver.Quiesce();
}

namespace {

    template<typename Driver>
    typename Driver::REQ PersistenceRequest(Driver& driver, uint16_t opcode, TransactionId txnId,
                                            uint64_t address, uint8_t group = 0)
    {
        auto request = driver.Request(opcode, 5, txnId, address);
        request.PGroupID() = group;
        request.Deep() = 1;
        return request;
    }

    template<typename config>
    void CheckPersistenceResponse(CheckedDriver<config>& driver, uint16_t opcode, TransactionId txnId, uint8_t group)
    {
        if (opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep)
        {
            auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompPersist);
            LUNATIC_CHECK(response.TxnID() == txnId && response.PGroupID() == group);
            LUNATIC_CHECK(response.Resp() == CHI::Eb::Resps::Comp_UC);
        }
        else
        {
            auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
            LUNATIC_CHECK(response.TxnID() == txnId && response.Resp() == CHI::Eb::Resps::Comp_UC);
        }
    }

    void RunPersistenceState(PolicyKind policy, uint16_t opcode, CacheState state)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x4b000, false);
        const LineData data = PersistencePattern();
        driver.driver.node.GetMemory().WriteFull(line, data);
        if (state == CacheState::SC)
            LUNATIC_CHECK(driver.ReadData(5, 10, line.address, CHI::Eb::Opcodes::REQ::ReadShared) == data);
        else if (state == CacheState::UC)
            LUNATIC_CHECK(driver.ReadData(5, 10, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        const uint8_t group = opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep ? 0x44 : 0;
        driver.PushRequest(PersistenceRequest(driver, opcode, 11, line.address, group));
        CheckPersistenceResponse(driver, opcode, 11, group);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == state);
    }

    void RunDirtyPersistence(PolicyKind policy, uint16_t opcode)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x4b100, false);
        const LineData data = PersistencePattern();
        SeedDirtyHome(driver, line, data);
        LUNATIC_CHECK(driver.ReadData(6, 20, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UD);

        const uint8_t group = opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep ? 0x55 : 0;
        driver.PushRequest(PersistenceRequest(driver, opcode, 21, line.address, group));
        auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanShared);
        LUNATIC_CHECK(snoop.target == 6 && snoop.flit.RetToSrc() == 0);
        const bool invalidate = opcode == CHI::Eb::Opcodes::REQ::CleanSharedPersistSep;
        driver.SnoopData(6, snoop.flit.TxnID(), data,
            invalidate ? CHI::Eb::Resps::SnpRespData_I_PD : CHI::Eb::Resps::SnpRespData_UC_PD);
        CheckPersistenceResponse(driver, opcode, 21, group);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == data);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::I);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == (invalidate ? CacheState::I : CacheState::UC));
        LUNATIC_CHECK(!driver.driver.node.InspectHome(line).valid || !driver.driver.node.InspectHome(line).dirty);
    }

}

LUNATIC_TEST(Stage5PersistenceRequesterStates)
{
    const PolicyKind policies[] = { PolicyKind::NINE, PolicyKind::Exclusive, PolicyKind::Inclusive };
    const CacheState states[] = { CacheState::I, CacheState::SC, CacheState::UC };
    const uint16_t opcodes[] = {
        CHI::Eb::Opcodes::REQ::CleanSharedPersist, CHI::Eb::Opcodes::REQ::CleanSharedPersistSep
    };
    for (PolicyKind policy : policies)
        for (uint16_t opcode : opcodes)
            for (CacheState state : states)
                RunPersistenceState(policy, opcode, state);
}

LUNATIC_TEST(Stage5PersistenceDirtyPeers)
{
    const PolicyKind policies[] = { PolicyKind::NINE, PolicyKind::Exclusive, PolicyKind::Inclusive };
    for (PolicyKind policy : policies)
    {
        RunDirtyPersistence(policy, CHI::Eb::Opcodes::REQ::CleanSharedPersist);
        RunDirtyPersistence(policy, CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
    }
}

LUNATIC_TEST(Stage5PersistenceImpreciseFilterSnoopsCleanPeers)
{
    Options options;
    options.preciseSnoopFilter = false;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x4b200, false);
    const LineData data = PersistencePattern();
    driver.driver.node.GetMemory().WriteFull(line, data);
    LUNATIC_CHECK(driver.ReadData(6, 30, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
    driver.PushRequest(PersistenceRequest(driver, CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 31, line.address, 0x66));
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanShared);
    LUNATIC_CHECK(snoop.target == 6);
    driver.SnoopResponse(6, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_UC);
    CheckPersistenceResponse(driver, CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 31, 0x66);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 6) == CacheState::UC);
}

LUNATIC_TEST(Stage5PersistenceRejectsInvalidRequests)
{
    {
        Driver<Config256> driver;
        auto request = PersistenceRequest(driver, CHI::Eb::Opcodes::REQ::CleanSharedPersist, 40, 0x4b300, 0x77);
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(comp.RespErr() == 0b10);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
    }
    {
        Driver<Config256> driver;
        auto request = PersistenceRequest(driver, CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 41, 0x4b300, 0x77);
        request.Size() = 4;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(comp.RespErr() == 0b10);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
    }
}
