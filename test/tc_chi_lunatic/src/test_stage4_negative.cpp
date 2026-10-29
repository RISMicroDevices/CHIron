#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData                            NegativeData(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 5);
        return data;
    }

    ByteEnables                         NegativeEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    void                                NegativeWriteback(CheckedDriver<Config256>& driver, const LineKey& line, const LineData& data)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 70, line.address));
        auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(4, 70, dbid.DBID(), data, NegativeEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
                         CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
    }

    LineData                            NegativeReceive(CheckedDriver<Config256>& driver, NodeId source, TransactionId txnId, uint8_t resp,
                                                        bool compAck = true)
    {
        LineData result{};
        DatabaseId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.Resp() == resp);
            dbid = data.DBID();
            const size_t offset = size_t(data.DataID()) * 16;
            for (size_t index = 0; index < 32; ++index)
                result[offset + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
        }
        if (compAck)
            driver.CompAck(source, dbid);
        return result;
    }

    void                                RawSnoopData(Driver<Config256>& driver, NodeId source, TransactionId txnId,
                                                     const LineData& line, uint8_t resp)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            Driver<Config256>::DAT data{};
            data.SrcID() = source;
            data.TgtID() = 2;
            data.TxnID() = txnId;
            data.Opcode() = CHI::Eb::Opcodes::DAT::SnpRespData;
            data.Resp() = resp;
            data.DataID() = beat * 2;
            for (size_t index = 0; index < 32; ++index)
            {
                data.Data()[index / 8] |= uint64_t(line[beat * 32 + index]) << ((index % 8) * 8);
                data.BE() = uint64_t(data.BE()) | (uint64_t(1) << index);
            }
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
    }

    void                                PartialDirtyMatrix(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x11000, false);
        const LineData expected = NegativeData(37);
        NegativeWriteback(driver, line, expected);
        LUNATIC_CHECK(driver.ReadData(5, 71, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == expected);
        driver.Quiesce();

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 72, line.address, true));
        auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
        driver.SnoopData(5, snoop.flit.TxnID(), expected, CHI::Eb::Resps::SnpRespDataPtl_I_PD, true);
        LUNATIC_CHECK(NegativeReceive(driver, 6, 72, CHI::Eb::Resps::CompData_UD_PD) == expected);
        driver.Quiesce();
    }

}

LUNATIC_TEST(Stage4PartialDirtySnoopAllPolicies)
{
    PartialDirtyMatrix(PolicyKind::NINE);
    PartialDirtyMatrix(PolicyKind::Exclusive);
    PartialDirtyMatrix(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage4DoNotGoToSDViolationIsReported)
{
    Options options;
    options.doNotGoToSD = true;
    Driver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x12000, false);
    const LineData data = NegativeData(43);
    driver.node.GetMemory().WriteFull(line, data);
    LUNATIC_CHECK(driver.ReadData(4, 80, line.address, CHI::Eb::Opcodes::REQ::ReadUnique, true) == data);
    driver.Quiesce();

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 5, 81, line.address, true)));
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
    RawSnoopData(driver, 4, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_SD);
    auto first = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.CompAck(5, first.DBID());
    driver.Quiesce();
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage4SnpOnceMayRetainSD)
{
    Options options;
    options.doNotGoToSD = true;
    CheckedDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x13000, false);
    const LineData data = NegativeData(53);
    NegativeWriteback(driver, line, data);
    LUNATIC_CHECK(driver.ReadData(5, 82, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
    driver.Quiesce();

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadOnce, 6, 83, line.address));
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpOnce);
    driver.SnoopData(5, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_SD);
    LUNATIC_CHECK(NegativeReceive(driver, 6, 83, CHI::Eb::Resps::CompData_UC, false) == data);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::SD);
}

LUNATIC_TEST(Stage4DuplicateLiveRequestIDIsDenied)
{
    Options options;
    options.memoryLatency = 32;
    Driver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0x14000, false);
    driver.node.GetMemory().WriteFull(line, NegativeData(61));
    auto first = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 90, line.address);
    LUNATIC_CHECK(driver.node.PushRXREQ(first));
    driver.Tick();
    first.Addr() = line.address + 0x1000;
    LUNATIC_CHECK(driver.node.PushRXREQ(first));
    driver.Until([&] { return !driver.node.GetErrors().empty(); });
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.Quiesce();
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage4SnoopIDCapacityIsValidated)
{
    Options options;
    options.transactionLimit = 4096;
    options.snoopLimit = 32;
    CheckThrows([&] { ValidateOptions(options); });
    options.transactionLimit = 128;
    options.snoopLimit = 32;
    ValidateOptions(options);
}
