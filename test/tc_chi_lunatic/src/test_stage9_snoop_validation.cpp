#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    void PushRequest(Driver<Config256>& driver, const Driver<Config256>::REQ& request)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
    }

    void SnoopData(Driver<Config256>& driver, NodeId source, TransactionId txnId,
        const LineData& line, uint8_t resp)
    {
        constexpr size_t bytes = Config256::dataWidth / 8;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            Driver<Config256>::DAT data{};
            data.SrcID() = source;
            data.TgtID() = driver.node.GetNodeId();
            data.TxnID() = txnId;
            data.Opcode() = CHI::Eb::Opcodes::DAT::SnpRespData;
            data.Resp() = resp;
            data.DataID() = beat * 2;
            data.CCID() = beat % 2;
            for (size_t index = 0; index < bytes; ++index)
            {
                data.Data()[index / 8] |= uint64_t(line[beat * bytes + index]) << ((index % 8) * 8);
                data.BE() |= uint64_t(1) << index;
            }
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
    }

    void WriteData(Driver<Config256>& driver, TransactionId txnId, DataBufferId dbid, const LineData& line)
    {
        constexpr size_t bytes = Config256::dataWidth / 8;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            Driver<Config256>::DAT data{};
            data.SrcID() = 1;
            data.TgtID() = driver.node.GetNodeId();
            data.TxnID() = dbid;
            data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
            data.DataID() = beat * 2;
            data.CCID() = beat % 2;
            for (size_t index = 0; index < bytes; ++index)
            {
                data.Data()[index / 8] |= uint64_t(line[beat * bytes + index]) << ((index % 8) * 8);
                data.BE() |= uint64_t(1) << index;
            }
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
        (void)txnId;
    }

    void CompleteStash(Driver<Config256>& driver, TransactionId txnId, const LineData& line)
    {
        driver.Until([&] { return static_cast<bool>(driver.node.PeekTXRSP()); });
        const uint16_t firstOpcode = driver.node.PeekTXRSP()->Opcode();
        if (firstOpcode == CHI::Eb::Opcodes::RSP::Comp)
        {
            LUNATIC_CHECK(driver.NextRSP(firstOpcode).TxnID() == txnId);
            const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
            WriteData(driver, txnId, dbid, line);
        }
        else if (firstOpcode == CHI::Eb::Opcodes::RSP::DBIDResp)
        {
            const DataBufferId dbid = driver.NextRSP(firstOpcode).DBID();
            WriteData(driver, txnId, dbid, line);
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == txnId);
        }
        else
        {
            LUNATIC_CHECK(firstOpcode == CHI::Eb::Opcodes::RSP::CompDBIDResp);
            const DataBufferId dbid = driver.NextRSP(firstOpcode).DBID();
            WriteData(driver, txnId, dbid, line);
        }
    }

    void ReadLine(Driver<Config256>& driver, NodeId source, TransactionId txnId,
        uint16_t opcode, uint64_t address)
    {
        PushRequest(driver, driver.Request(opcode, source, txnId, address, true));
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            dbid = data.DBID();
        }
        driver.CompAck(source, dbid);
    }

    void CleanUniqueLine(Driver<Config256>& driver, NodeId source, TransactionId txnId, uint64_t address)
    {
        PushRequest(driver, driver.Request(CHI::Eb::Opcodes::REQ::CleanUnique, source, txnId, address, true));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.CompAck(source, response.DBID());
    }

    LineData DirtyPattern()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(index * 5 + 3);
        return data;
    }

    DataBufferId CheckDirtyRead(Driver<Config256>& driver, const LineData& expected)
    {
        DataBufferId dbid = 0;
        constexpr size_t bytes = Config256::dataWidth / 8;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            dbid = data.DBID();
            for (size_t index = 0; index < bytes; ++index)
                LUNATIC_CHECK(uint8_t(data.Data()[index / 8] >> ((index % 8) * 8)) == expected[beat * bytes + index]);
        }
        return dbid;
    }

}

LUNATIC_TEST(Stage9SnoopUniqueStashAcceptsDirtyAfterUC)
{
    Driver<Config256> driver;
    ReadLine(driver, 4, 1, CHI::Eb::Opcodes::REQ::ReadUnique, 0x19000);

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteUniquePtlStash, 1, 2, 0x19000);
    request.ReturnNID() = 4;
    request.StashNIDValid() = 1;
    request.StashLPIDValid() = 1;
    request.StashLPID() = 3;
    PushRequest(driver, request);
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUniqueStash);
    LUNATIC_CHECK(snoop.target == 4);
    SnoopData(driver, 4, snoop.flit.TxnID(), DirtyPattern(), CHI::Eb::Resps::SnpRespData_I_PD);
    CompleteStash(driver, 2, DirtyPattern());
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9SnoopNotSharedDirtyAcceptsDirtyAfterUC)
{
    Driver<Config256> driver;
    const auto expected = DirtyPattern();
    ReadLine(driver, 4, 1, CHI::Eb::Opcodes::REQ::ReadUnique, 0x19000);
    PushRequest(driver, driver.Request(CHI::Eb::Opcodes::REQ::ReadNotSharedDirty, 1, 2, 0x19000, true));

    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpNotSharedDirty);
    LUNATIC_CHECK(snoop.target == 4);
    SnoopData(driver, 4, snoop.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_I_PD);
    driver.CompAck(1, CheckDirtyRead(driver, expected));
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9SnoopRejectsDirtyFromSC)
{
    Driver<Config256> driver;
    ReadLine(driver, 4, 1, CHI::Eb::Opcodes::REQ::ReadShared, 0x19000);
    PushRequest(driver, driver.Request(CHI::Eb::Opcodes::REQ::ReadNotSharedDirty, 1, 2, 0x19000, true));

    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpNotSharedDirty);
    SnoopData(driver, 4, snoop.flit.TxnID(), DirtyPattern(), CHI::Eb::Resps::SnpRespData_I_PD);
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9SnoopRejectsDirtyFromUCE)
{
    Driver<Config256> driver;
    CleanUniqueLine(driver, 4, 1, 0x19000);
    PushRequest(driver, driver.Request(CHI::Eb::Opcodes::REQ::ReadNotSharedDirty, 1, 2, 0x19000, true));

    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpNotSharedDirty);
    SnoopData(driver, 4, snoop.flit.TxnID(), DirtyPattern(), CHI::Eb::Resps::SnpRespData_I_PD);
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9SnoopPassDirtyAfterUCUpdatesMemory)
{
    Driver<Config256> driver;
    const LineKey line = LineKey::FromAddress(0x19000, false);
    const auto expected = DirtyPattern();
    ReadLine(driver, 4, 1, CHI::Eb::Opcodes::REQ::ReadUnique, line.address);
    LUNATIC_CHECK(driver.node.InspectRequester(line, 4) == CacheState::UC);

    PushRequest(driver, driver.Request(CHI::Eb::Opcodes::REQ::ReadNotSharedDirty, 1, 2, line.address, true));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpNotSharedDirty);
    SnoopData(driver, 4, snoop.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespData_SC_PD);
    driver.CompAck(1, CheckDirtyRead(driver, expected));
    driver.Quiesce();

    LUNATIC_CHECK(driver.node.InspectRequester(line, 4) == CacheState::SC);
    LUNATIC_CHECK(driver.node.InspectRequester(line, 1) == CacheState::SC);
    LUNATIC_CHECK(driver.node.GetMemory().Read(line) == expected);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9CopybackInvalidResponsePreservesMemory)
{
    Driver<Config256> driver;
    const LineKey line = LineKey::FromAddress(0x19100, false);
    const auto expected = DirtyPattern();
    driver.node.GetMemory().WriteFull(line, expected);

    PushRequest(driver, driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 3, line.address));
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    const ByteEnables noBytes{};
    driver.WriteData(4, 3, response.DBID(), expected, noBytes,
        CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_I);
    driver.Quiesce();

    LUNATIC_CHECK(!driver.node.InspectHome(line).valid);
    LUNATIC_CHECK(driver.node.GetMemory().Read(line) == expected);
    LUNATIC_CHECK(driver.node.InspectRequester(line, 4) == CacheState::I);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9CopybackWithRemainingSharerWritesThrough)
{
    Driver<Config256> driver;
    const LineKey line = LineKey::FromAddress(0x19200, false);
    const auto expected = DirtyPattern();
    driver.node.GetMemory().WriteFull(line, expected);
    ReadLine(driver, 4, 1, CHI::Eb::Opcodes::REQ::ReadShared, line.address);

    PushRequest(driver, driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 5, 2, line.address, true));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
    Driver<Config256>::RSP snoopResponse{};
    snoopResponse.SrcID() = 4;
    snoopResponse.TgtID() = driver.node.GetNodeId();
    snoopResponse.TxnID() = snoop.flit.TxnID();
    snoopResponse.Opcode() = CHI::Eb::Opcodes::RSP::SnpResp;
    snoopResponse.Resp() = CHI::Eb::Resps::SnpResp_SC;
    LUNATIC_CHECK(driver.node.PushRXRSP(snoopResponse));
    driver.Tick();
    driver.CompAck(5, CheckDirtyRead(driver, expected));
    driver.Quiesce();

    PushRequest(driver, driver.Request(CHI::Eb::Opcodes::REQ::WriteEvictOrEvict, 5, 3, line.address));
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    ByteEnables allBytes{};
    allBytes.fill(1);
    driver.WriteData(5, 3, response.DBID(), expected, allBytes,
        CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_SC);
    driver.Quiesce();

    LUNATIC_CHECK(!driver.node.InspectHome(line).valid);
    LUNATIC_CHECK(driver.node.InspectRequester(line, 4) == CacheState::SC);
    LUNATIC_CHECK(driver.node.InspectRequester(line, 5) == CacheState::I);
    LUNATIC_CHECK(driver.node.GetMemory().Read(line) == expected);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}
