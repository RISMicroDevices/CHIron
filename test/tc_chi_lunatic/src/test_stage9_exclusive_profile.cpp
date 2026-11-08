#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = CheckedDriver<Config256>;

    LineData ExclusivePattern()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(0x39 + index * 9);
        return data;
    }

    void CheckReadFamily(TestDriver& driver, uint16_t opcode, TransactionId txnId, uint64_t address)
    {
        auto request = driver.Request(opcode, 4, txnId, address, true);
        request.Excl() = 1;
        request.LPID() = 0x15;
        driver.PushRequest(request);
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.RespErr() == CHI::Eb::RespErrs::OK);
            dbid = data.DBID();
        }
        driver.CompAck(4, dbid);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9ExclusiveUnsupportedReadFamily)
{
    TransactionId txnId = 1;
    for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::ReadClean,
                                   CHI::Eb::Opcodes::REQ::ReadNotSharedDirty,
                                   CHI::Eb::Opcodes::REQ::ReadShared,
                                   CHI::Eb::Opcodes::REQ::ReadNoSnp })
    {
        TestDriver driver;
        CheckReadFamily(driver, opcode, txnId++, 0x77000);
    }
}

LUNATIC_TEST(Stage9ExclusiveUnsupportedWriteFamily)
{
    for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::WriteNoSnpFull,
                                   CHI::Eb::Opcodes::REQ::WriteNoSnpPtl })
    {
        TestDriver driver;
        const LineKey line = LineKey::FromAddress(0x77040, false);
        const LineData expected = ExclusivePattern();
        auto request = driver.Request(opcode, 4, 10, line.address);
        request.Excl() = 1;
        request.LPID() = 0x12;
        driver.PushRequest(request);
        const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
        LUNATIC_CHECK(dbid.RespErr() == CHI::Eb::RespErrs::OK);
        ByteEnables enables{};
        if (opcode == CHI::Eb::Opcodes::REQ::WriteNoSnpFull)
            enables.fill(1);
        else
            for (size_t index = 7; index < 23; ++index)
                enables[index] = 1;
        driver.WriteData(4, 10, dbid.DBID(), expected, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::OK);
        driver.Quiesce();
        LineData memory{};
        for (size_t index = 0; index < memory.size(); ++index)
            if (enables[index])
                memory[index] = expected[index];
        LUNATIC_CHECK(driver.driver.node.GetMemory().Read(line) == memory);
        LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
    }

    TestDriver driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanUnique, 4, 11, 0x77080, true);
    request.Excl() = 1;
    request.LPID() = 0x12;
    driver.PushRequest(request);
    const auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(comp.RespErr() == CHI::Eb::RespErrs::OK);
    driver.CompAck(4, comp.DBID());
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9ExclusiveUnsupportedLPIDPropagation)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    TestDriver driver(options);
    SNOracle<Config256> oracle(2, 6);
    oracle.RegisterRequester(4);

    auto read = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 20, 0x770c0);
    read.Excl() = 1;
    read.LPID() = 0x15;
    driver.PushRequest(read);
    const auto readDownstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    LUNATIC_CHECK(readDownstream.LPID() == 0x15);
    oracle.Request(driver.driver.now, readDownstream);
    for (size_t beat = 0; beat < 2; ++beat)
    {
        TestDriver::DAT data{};
        data.SrcID() = 6;
        data.TgtID() = 2;
        data.HomeNID() = 2;
        data.TxnID() = readDownstream.TxnID();
        data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data.Resp() = CHI::Eb::Resps::CompData_I;
        data.DataID() = beat * 2;
        data.CCID() = 0;
        data.BE() = ~uint64_t(0);
        oracle.Data(driver.driver.now, data);
        LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
    }
    driver.driver.Tick();
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).RespErr() == CHI::Eb::RespErrs::OK);

    auto write = driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 21, 0x77100);
    write.Excl() = 1;
    write.LPID() = 0x12;
    driver.PushRequest(write);
    const auto requesterDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
    LUNATIC_CHECK(requesterDBID.RespErr() == CHI::Eb::RespErrs::OK);
    ByteEnables enables{};
    enables.fill(1);
    driver.WriteData(4, 21, requesterDBID.DBID(), ExclusivePattern(), enables,
        CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    const auto writeDownstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    LUNATIC_CHECK(writeDownstream.LPID() == 0x12);
    oracle.Request(driver.driver.now, writeDownstream);

    TestDriver::RSP dbid{};
    dbid.SrcID() = 6;
    dbid.TgtID() = 2;
    dbid.TxnID() = writeDownstream.TxnID();
    dbid.DBID() = 77;
    dbid.Opcode() = CHI::Eb::Opcodes::RSP::DBIDResp;
    oracle.Response(driver.driver.now, dbid);
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(dbid));
    driver.driver.Tick();
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(data.TgtID() == 6 && data.TxnID() == 77);
        oracle.WriteData(driver.driver.now, data);
    }
    TestDriver::RSP comp = dbid;
    comp.Opcode() = CHI::Eb::Opcodes::RSP::Comp;
    comp.Resp() = CHI::Eb::Resps::Comp_I;
    oracle.Response(driver.driver.now, comp);
    LUNATIC_CHECK(driver.driver.node.PushRXRSP(comp));
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::OK);
    driver.Quiesce();
    LUNATIC_CHECK(!oracle.HasInflight());
    LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
}
