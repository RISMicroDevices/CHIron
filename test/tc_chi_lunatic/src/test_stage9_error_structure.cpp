#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using REQ        = TestDriver::REQ;
    using DAT        = TestDriver::DAT;

    REQ InvalidWrite(TestDriver& driver, uint16_t opcode, TransactionId txnId, uint64_t address)
    {
        auto request = driver.Request(opcode, 4, txnId, address);
        request.MemAttr() = 0x0c;
        request.AllowRetry() = 0;
        return request;
    }

    void CheckNoArchitecturalEffects(TestDriver& driver, uint64_t address)
    {
        const LineKey line = LineKey::FromAddress(address, false);
        LUNATIC_CHECK(!driver.node.InspectHome(line).valid);
        LUNATIC_CHECK(driver.node.GetMemory().Read(line) == LineData{});
        LUNATIC_CHECK(!driver.node.PeekTXREQ());
        LUNATIC_CHECK(!driver.node.PeekTXSNP());
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9ErrorStructureWriteNoSnpData)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6d000;
    LUNATIC_CHECK(driver.node.PushRXREQ(InvalidWrite(driver, CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 1, address)));
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    LineData data{};
    for (size_t index = 0; index < data.size(); ++index)
        data[index] = uint8_t(index + 1);
    ByteEnables enables{};
    enables.fill(1);
    driver.WriteData(4, 1, dbid, data, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
    driver.Quiesce();
    CheckNoArchitecturalEffects(driver, address);
}

LUNATIC_TEST(Stage9ErrorStructureWriteNoSnpCancel)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6d100;
    LUNATIC_CHECK(driver.node.PushRXREQ(InvalidWrite(driver, CHI::Eb::Opcodes::REQ::WriteNoSnpPtl, 2, address)));
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    for (size_t beat = 0; beat < 2; ++beat)
    {
        DAT cancel{};
        cancel.SrcID() = 4;
        cancel.TgtID() = 2;
        cancel.TxnID() = dbid;
        cancel.Opcode() = CHI::Eb::Opcodes::DAT::WriteDataCancel;
        cancel.DataID() = beat * 2;
        cancel.CCID() = 0;
        LUNATIC_CHECK(driver.node.PushRXDAT(cancel));
    }
    driver.Tick();
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
    driver.Quiesce();
    CheckNoArchitecturalEffects(driver, address);
}

namespace {

    void SendAtomicOperand(TestDriver& driver, DataBufferId dbid)
    {
        DAT operand{};
        operand.SrcID() = 4;
        operand.TgtID() = 2;
        operand.TxnID() = dbid;
        operand.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        operand.DataID() = 0;
        operand.CCID() = 0;
        operand.BE() = 0xff;
        operand.Data()[0] = 7;
        LUNATIC_CHECK(driver.node.PushRXDAT(operand));
        driver.Tick();
    }

    REQ AtomicErrorRequest(TestDriver& driver, uint16_t opcode, TransactionId txnId, uint64_t address)
    {
        auto request = driver.Request(opcode, 4, txnId, address);
        request.Size() = 3;
        request.ExpCompAck() = 1;
        request.AllowRetry() = 0;
        return request;
    }

}

LUNATIC_TEST(Stage9ErrorStructureAtomicSwap)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6d200;
    LUNATIC_CHECK(driver.node.PushRXREQ(AtomicErrorRequest(driver, CHI::Eb::Opcodes::REQ::AtomicSwap, 3, address)));
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    SendAtomicOperand(driver, dbid);
    const auto result = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(result.RespErr() == CHI::Eb::RespErrs::NDERR);
    LUNATIC_CHECK(result.TgtID() == 4 && result.TxnID() == 3 && result.DBID() == dbid);
    driver.Quiesce();
    CheckNoArchitecturalEffects(driver, address);
}

LUNATIC_TEST(Stage9ErrorStructureAtomicStore)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6d300;
    LUNATIC_CHECK(driver.node.PushRXREQ(AtomicErrorRequest(driver, CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 4, address)));
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    SendAtomicOperand(driver, dbid);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
    driver.Quiesce();
    CheckNoArchitecturalEffects(driver, address);
}

namespace {

    void CheckReadErrorData(TestDriver& driver, TransactionId txnId, bool ordered,
                            bool separated, bool compAck)
    {
        if (ordered)
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::ReadReceipt).RespErr() == CHI::Eb::RespErrs::OK);
        DataBufferId dbid = 0;
        if (separated)
        {
            const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
            LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
            dbid = response.DBID();
        }
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(separated ? CHI::Eb::Opcodes::DAT::DataSepResp
                                                       : CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.RespErr() == CHI::Eb::RespErrs::NDERR);
            LUNATIC_CHECK(data.TgtID() == 4 && data.TxnID() == txnId);
            dbid = data.DBID();
        }
        if (compAck)
            driver.CompAck(4, dbid);
    }

}

LUNATIC_TEST(Stage9ErrorStructureReadNoSnp)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6d400;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 5, address);
    request.SnpAttr() = 1;
    request.AllowRetry() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    CheckReadErrorData(driver, 5, false, false, false);
    driver.Quiesce();
    CheckNoArchitecturalEffects(driver, address);
}

LUNATIC_TEST(Stage9ErrorStructureReadUniqueCompAck)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6d500;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 6, address, true);
    request.MemAttr() = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
    request.AllowRetry() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    CheckReadErrorData(driver, 6, false, false, true);
    driver.Quiesce();
    CheckNoArchitecturalEffects(driver, address);
}

LUNATIC_TEST(Stage9ErrorStructureOrderedReadReceipt)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6d600;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, address);
    request.SnpAttr() = 1;
    request.Order() = 2;
    request.AllowRetry() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    CheckReadErrorData(driver, 7, true, false, false);
    driver.Quiesce();
    CheckNoArchitecturalEffects(driver, address);
}

LUNATIC_TEST(Stage9ErrorStructureSeparatedRead)
{
    Options options;
    options.separatedReadResponse = true;
    TestDriver driver(options);
    constexpr uint64_t address = 0x6d700;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 8, address, true);
    request.SnpAttr() = 1;
    request.AllowRetry() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    CheckReadErrorData(driver, 8, false, true, true);
    driver.Quiesce();
    CheckNoArchitecturalEffects(driver, address);
}

namespace {

    void CheckDatalessError(TestDriver& driver, const REQ& request, uint8_t resp)
    {
        const uint64_t address = request.Addr();
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
        LUNATIC_CHECK(response.Resp() == resp);
        if (request.ExpCompAck())
            driver.CompAck(request.SrcID(), response.DBID());
        driver.Quiesce();
        CheckNoArchitecturalEffects(driver, address);
    }

}

LUNATIC_TEST(Stage9ErrorStructureWriteBack)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6d800;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 9, address);
    request.MemAttr() = 0x0c;
    request.AllowRetry() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
    ByteEnables enables{};
    enables.fill(1);
    driver.WriteData(4, 9, response.DBID(), LineData{}, enables, CHI::Eb::Opcodes::DAT::CopyBackWrData,
                     CHI::Eb::Resps::CopyBackWrData_UD_PD);
    driver.Quiesce();
    CheckNoArchitecturalEffects(driver, address);
}

LUNATIC_TEST(Stage9ErrorStructureCleanUnique)
{
    TestDriver driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanUnique, 4, 10, 0x6d900, true);
    request.MemAttr() = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
    request.AllowRetry() = 0;
    CheckDatalessError(driver, request, CHI::Eb::Resps::Comp_UC);
}

LUNATIC_TEST(Stage9ErrorStructureCleanInvalid)
{
    TestDriver driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanInvalid, 4, 11, 0x6da00);
    request.MemAttr() = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
    request.AllowRetry() = 0;
    CheckDatalessError(driver, request, CHI::Eb::Resps::Comp_I);
}

LUNATIC_TEST(Stage9ErrorStructureMakeUnique)
{
    TestDriver driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::MakeUnique, 4, 12, 0x6db00, true);
    request.MemAttr() = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
    request.AllowRetry() = 0;
    CheckDatalessError(driver, request, CHI::Eb::Resps::Comp_UC);
}

LUNATIC_TEST(Stage9ErrorStructureEvict)
{
    TestDriver driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::Evict, 4, 13, 0x6dc00);
    request.MemAttr() = 0x0c;
    request.AllowRetry() = 0;
    CheckDatalessError(driver, request, CHI::Eb::Resps::Comp_I);
}

LUNATIC_TEST(Stage9ErrorStructureWriteUniqueData)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6dd00;
    LUNATIC_CHECK(driver.node.PushRXREQ(InvalidWrite(driver, CHI::Eb::Opcodes::REQ::WriteUniqueFull, 14, address)));
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    ByteEnables enables{};
    enables.fill(1);
    driver.WriteData(4, 14, dbid, LineData{}, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
    driver.Quiesce();
    CheckNoArchitecturalEffects(driver, address);
}

LUNATIC_TEST(Stage9ErrorStructureWriteUniqueCancel)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6de00;
    LUNATIC_CHECK(driver.node.PushRXREQ(InvalidWrite(driver, CHI::Eb::Opcodes::REQ::WriteUniquePtl, 15, address)));
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    for (size_t beat = 0; beat < 2; ++beat)
    {
        DAT cancel{};
        cancel.SrcID() = 4;
        cancel.TgtID() = 2;
        cancel.TxnID() = dbid;
        cancel.Opcode() = CHI::Eb::Opcodes::DAT::WriteDataCancel;
        cancel.DataID() = beat * 2;
        cancel.CCID() = 0;
        LUNATIC_CHECK(driver.node.PushRXDAT(cancel));
    }
    driver.Tick();
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
    driver.Quiesce();
    CheckNoArchitecturalEffects(driver, address);
}

LUNATIC_TEST(Stage9ErrorStructureWriteEvictOrEvictNoData)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6df00;
    LUNATIC_CHECK(driver.node.PushRXREQ(InvalidWrite(driver, CHI::Eb::Opcodes::REQ::WriteEvictOrEvict, 16, address)));
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
    driver.CompAck(4, response.DBID());
    driver.Quiesce();
    CheckNoArchitecturalEffects(driver, address);
}

namespace {

    void CheckZeroWriteError(uint16_t opcode, TransactionId txnId, uint64_t address)
    {
        TestDriver driver;
        auto request = driver.Request(opcode, 4, txnId, address);
        request.MemAttr() = 0x0c;
        request.AllowRetry() = 0;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
        LUNATIC_CHECK(response.TgtID() == 4 && response.TxnID() == txnId);
        driver.Quiesce();
        CheckNoArchitecturalEffects(driver, address);
    }

}

LUNATIC_TEST(Stage9ErrorStructureWriteNoSnpZero)
{
    CheckZeroWriteError(CHI::Eb::Opcodes::REQ::WriteNoSnpZero, 17, 0x6e000);
}

LUNATIC_TEST(Stage9ErrorStructureWriteUniqueZero)
{
    CheckZeroWriteError(CHI::Eb::Opcodes::REQ::WriteUniqueZero, 18, 0x6e100);
}

namespace {

    void CheckPersistenceError(uint16_t opcode, TransactionId txnId, uint64_t address, uint8_t group)
    {
        TestDriver driver;
        auto request = driver.Request(opcode, 4, txnId, address);
        request.MemAttr() = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
        request.PGroupID() = group;
        request.AllowRetry() = 0;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompPersist);
        LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
        LUNATIC_CHECK(response.PGroupID() == group && response.TxnID() == txnId);
        driver.Quiesce();
        CheckNoArchitecturalEffects(driver, address);
    }

    void CheckStashError(uint16_t opcode, TransactionId txnId, uint64_t address, bool separated)
    {
        TestDriver driver;
        auto request = driver.Request(opcode, 4, txnId, address);
        request.MemAttr() = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
        request.ReturnNID() = 5;
        request.StashNIDValid() = 1;
        request.StashLPIDValid() = 1;
        request.StashLPID() = 3;
        request.StashGroupID() = 0x71;
        request.AllowRetry() = 0;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        if (separated)
        {
            const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompStashDone);
            LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
            LUNATIC_CHECK(response.StashGroupID() == 0x71 && response.TxnID() == txnId);
        }
        else
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
        driver.Quiesce();
        CheckNoArchitecturalEffects(driver, address);
    }

}

LUNATIC_TEST(Stage9ErrorStructureCleanSharedPersist)
{
    CheckPersistenceError(CHI::Eb::Opcodes::REQ::CleanSharedPersist, 19, 0x6e200, 0x55);
}

LUNATIC_TEST(Stage9ErrorStructureCleanSharedPersistSep)
{
    CheckPersistenceError(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 20, 0x6e300, 0x66);
}

LUNATIC_TEST(Stage9ErrorStructureStashOnceShared)
{
    CheckStashError(CHI::Eb::Opcodes::REQ::StashOnceShared, 21, 0x6e400, false);
}

LUNATIC_TEST(Stage9ErrorStructureStashOnceSepShared)
{
    CheckStashError(CHI::Eb::Opcodes::REQ::StashOnceSepShared, 22, 0x6e500, true);
}

LUNATIC_TEST(Stage9ErrorStructureReservedOrderStaysContained)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6e600;
    auto request = InvalidWrite(driver, CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 23, address);
    request.Order() = 1;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::DERR);
    driver.Quiesce();
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
}

namespace {

    REQ ValidTargetWriteUniqueStash(TestDriver& driver, uint16_t opcode, TransactionId txnId, uint64_t address)
    {
        auto request = InvalidWrite(driver, opcode, txnId, address);
        request.ReturnNID() = 5;
        request.StashNIDValid() = 1;
        request.StashLPIDValid() = 1;
        request.StashLPID() = 3;
        return request;
    }

}

LUNATIC_TEST(Stage9ErrorStructureWriteUniqueFullStash)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6e700;
    LUNATIC_CHECK(driver.node.PushRXREQ(ValidTargetWriteUniqueStash(
        driver, CHI::Eb::Opcodes::REQ::WriteUniqueFullStash, 24, address)));
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    ByteEnables enables{};
    enables.fill(1);
    driver.WriteData(4, 24, dbid, LineData{}, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
    driver.Quiesce();
    CheckNoArchitecturalEffects(driver, address);
}

LUNATIC_TEST(Stage9ErrorStructureWriteUniquePtlStash)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6e800;
    LUNATIC_CHECK(driver.node.PushRXREQ(ValidTargetWriteUniqueStash(
        driver, CHI::Eb::Opcodes::REQ::WriteUniquePtlStash, 25, address)));
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    for (size_t beat = 0; beat < 2; ++beat)
    {
        DAT cancel{};
        cancel.SrcID() = 4;
        cancel.TgtID() = 2;
        cancel.TxnID() = dbid;
        cancel.Opcode() = CHI::Eb::Opcodes::DAT::WriteDataCancel;
        cancel.DataID() = beat * 2;
        cancel.CCID() = 0;
        LUNATIC_CHECK(driver.node.PushRXDAT(cancel));
    }
    driver.Tick();
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
    driver.Quiesce();
    CheckNoArchitecturalEffects(driver, address);
}
