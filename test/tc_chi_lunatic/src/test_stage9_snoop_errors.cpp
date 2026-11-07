#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using REQ        = TestDriver::REQ;
    using RSP        = TestDriver::RSP;
    using DAT        = TestDriver::DAT;

    LineData ErrorLineData()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(0x31 + index * 7);
        return data;
    }

    void SetPacketData(DAT& packet, const LineData& data, size_t beat)
    {
        constexpr size_t bytes = Config256::dataWidth / 8;
        for (size_t index = 0; index < bytes; ++index)
            packet.Data()[index / 8] |= uint64_t(data[beat * bytes + index]) << ((index % 8) * 8);
        packet.BE() = 0xffffffff;
    }

    void SeedDirtyOwner(TestDriver& driver, uint64_t address, const LineData& data, bool externalBackend = false)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, address)));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        for (size_t beat = 0; beat < 2; ++beat)
        {
            DAT packet{};
            packet.SrcID() = 6;
            packet.TgtID() = 2;
            packet.TxnID() = response.DBID();
            packet.Opcode() = CHI::Eb::Opcodes::DAT::CopyBackWrData;
            packet.Resp() = CHI::Eb::Resps::CopyBackWrData_UD_PD;
            packet.DataID() = beat * 2;
            SetPacketData(packet, data, beat);
            LUNATIC_CHECK(driver.node.PushRXDAT(packet));
        }
        driver.Tick();
        driver.Quiesce();

        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 91, address, true)));
        if (externalBackend)
        {
            driver.Until([&] { return static_cast<bool>(driver.node.PeekTXREQ() || driver.node.PeekTXDAT()); });
            if (driver.node.PeekTXREQ())
            {
                const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
                for (size_t beat = 0; beat < 2; ++beat)
                {
                    DAT packet{};
                    packet.SrcID() = 6;
                    packet.TgtID() = 2;
                    packet.HomeNID() = 2;
                    packet.TxnID() = downstream.TxnID();
                    packet.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
                    packet.Resp() = CHI::Eb::Resps::CompData_I;
                    packet.DataID() = beat * 2;
                    packet.CCID() = 0;
                    SetPacketData(packet, data, beat);
                    LUNATIC_CHECK(driver.node.PushRXDAT(packet));
                }
                driver.Tick();
            }
        }
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
            dbid = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).DBID();
        driver.CompAck(6, dbid);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.InspectRequester(LineKey::FromAddress(address, false), 6) == CacheState::UD);
    }

}

LUNATIC_TEST(Stage9SnoopErrorDirtyDataPreservesRequesterBeats)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6b000;
    const LineData data = ErrorLineData();
    SeedDirtyOwner(driver, address, data);

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 1, address, true)));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(snoop.target == 6);
    for (size_t beat = 0; beat < 2; ++beat)
    {
        DAT packet{};
        packet.SrcID() = 6;
        packet.TgtID() = 2;
        packet.TxnID() = snoop.flit.TxnID();
        packet.Opcode() = CHI::Eb::Opcodes::DAT::SnpRespData;
        packet.Resp() = CHI::Eb::Resps::SnpRespData_I_PD;
        packet.RespErr() = beat == 0 ? CHI::Eb::RespErrs::DERR : CHI::Eb::RespErrs::OK;
        packet.DataID() = beat * 2;
        packet.CCID() = 0;
        SetPacketData(packet, data, beat);
        LUNATIC_CHECK(driver.node.PushRXDAT(packet));
    }
    driver.Tick();

    DataBufferId dbid = 0;
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto response = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(response.RespErr() == (beat == 0 ? CHI::Eb::RespErrs::DERR : CHI::Eb::RespErrs::OK));
        dbid = response.DBID();
    }
    driver.CompAck(4, dbid);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.InspectRequester(LineKey::FromAddress(address, false), 6) == CacheState::I);
}

namespace {

    void SendBackendResponse(TestDriver& driver, const REQ& request, uint16_t opcode, uint8_t respErr)
    {
        RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = request.TxnID();
        response.DBID() = 77;
        response.Opcode() = opcode;
        response.Resp() = CHI::Eb::Resps::Comp_I;
        response.RespErr() = respErr;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

}

LUNATIC_TEST(Stage9SnoopErrorDirtyDataPropagatesToBackendWrite)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.policy = PolicyKind::Inclusive;
    TestDriver driver(options);
    constexpr uint64_t address = 0x6b100;
    const LineData data = ErrorLineData();
    SeedDirtyOwner(driver, address, data, true);

    auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicSwap, 4, 2, address);
    request.Size() = 0;
    request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    request.SnpAttr() = 1;
    request.AllowRetry() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    LUNATIC_CHECK(snoop.target == 6);
    for (size_t beat = 0; beat < 2; ++beat)
    {
        DAT packet{};
        packet.SrcID() = 6;
        packet.TgtID() = 2;
        packet.TxnID() = snoop.flit.TxnID();
        packet.Opcode() = CHI::Eb::Opcodes::DAT::SnpRespData;
        packet.Resp() = CHI::Eb::Resps::SnpRespData_I_PD;
        packet.RespErr() = beat == 0 ? CHI::Eb::RespErrs::DERR : CHI::Eb::RespErrs::OK;
        packet.DataID() = beat * 2;
        packet.CCID() = 0;
        SetPacketData(packet, data, beat);
        LUNATIC_CHECK(driver.node.PushRXDAT(packet));
    }
    driver.Tick();

    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
    DAT operand{};
    operand.SrcID() = 4;
    operand.TgtID() = 2;
    operand.TxnID() = dbid;
    operand.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
    operand.DataID() = 0;
    operand.CCID() = 0;
    operand.BE() = 1;
    operand.Data()[0] = 7;
    LUNATIC_CHECK(driver.node.PushRXDAT(operand));
    driver.Tick();

    const auto downstreamWrite = driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    SendBackendResponse(driver, downstreamWrite, CHI::Eb::Opcodes::RSP::DBIDResp, CHI::Eb::RespErrs::OK);
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto writeData = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(writeData.RespErr() == (beat == 0 ? CHI::Eb::RespErrs::DERR : CHI::Eb::RespErrs::OK));
    }
    SendBackendResponse(driver, downstreamWrite, CHI::Eb::Opcodes::RSP::Comp, CHI::Eb::RespErrs::OK);

    const auto downstreamAtomic = driver.NextREQ(CHI::Eb::Opcodes::REQ::AtomicSwap);
    SendBackendResponse(driver, downstreamAtomic, CHI::Eb::Opcodes::RSP::DBIDResp, CHI::Eb::RespErrs::OK);
    LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData).RespErr() == CHI::Eb::RespErrs::OK);
    DAT result{};
    result.SrcID() = 6;
    result.TgtID() = 2;
    result.HomeNID() = 2;
    result.TxnID() = downstreamAtomic.TxnID();
    result.DBID() = 77;
    result.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    result.Resp() = CHI::Eb::Resps::CompData_I;
    result.DataID() = 0;
    result.CCID() = 0;
    result.BE() = 1;
    result.Data()[0] = 9;
    LUNATIC_CHECK(driver.node.PushRXDAT(result));
    driver.Tick();
    LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).RespErr() == CHI::Eb::RespErrs::DERR);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

namespace {

    void SendNDERRResponse(TestDriver& driver, TransactionId txnId)
    {
        RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = txnId;
        response.Opcode() = CHI::Eb::Opcodes::RSP::SnpResp;
        response.Resp() = CHI::Eb::Resps::SnpResp_I;
        response.RespErr() = CHI::Eb::RespErrs::NDERR;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    void CheckNDERRReadData(TestDriver& driver)
    {
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto response = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
            dbid = response.DBID();
        }
        driver.CompAck(4, dbid);
    }

}

LUNATIC_TEST(Stage9SnoopErrorNDERRResponsePropagates)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6b200;
    SeedDirtyOwner(driver, address, ErrorLineData());
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 3, address, true)));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    SendNDERRResponse(driver, snoop.flit.TxnID());
    CheckNDERRReadData(driver);
    driver.Quiesce();
    const LineKey line = LineKey::FromAddress(address, false);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.InspectRequester(line, 6) == CacheState::I);
    LUNATIC_CHECK(driver.node.InspectRequester(line, 4) == CacheState::I);
}

LUNATIC_TEST(Stage9SnoopErrorNDERRDataIsRejected)
{
    TestDriver driver;
    constexpr uint64_t address = 0x6b300;
    const LineData data = ErrorLineData();
    SeedDirtyOwner(driver, address, data);
    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 4, 4, address, true)));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUnique);
    DAT invalid{};
    invalid.SrcID() = 6;
    invalid.TgtID() = 2;
    invalid.TxnID() = snoop.flit.TxnID();
    invalid.Opcode() = CHI::Eb::Opcodes::DAT::SnpRespData;
    invalid.Resp() = CHI::Eb::Resps::SnpRespData_I_PD;
    invalid.RespErr() = CHI::Eb::RespErrs::NDERR;
    invalid.DataID() = 0;
    invalid.CCID() = 0;
    SetPacketData(invalid, data, 0);
    LUNATIC_CHECK(driver.node.PushRXDAT(invalid));
    driver.Tick();
    const LineKey line = LineKey::FromAddress(address, false);
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.InspectRequester(line, 6) == CacheState::UD);

    SendNDERRResponse(driver, snoop.flit.TxnID());
    CheckNDERRReadData(driver);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.InspectRequester(line, 6) == CacheState::I);
    LUNATIC_CHECK(driver.node.InspectRequester(line, 4) == CacheState::I);
}

LUNATIC_TEST(Stage9SnoopErrorDCTNDERRFallsBack)
{
    Options options;
    options.dctEnabled = true;
    TestDriver driver(options);
    constexpr uint64_t address = 0x6b400;
    SeedDirtyOwner(driver, address, ErrorLineData());
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 5, address, true);
    request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpSharedFwd);
    LUNATIC_CHECK(snoop.flit.FwdNID() == 4 && snoop.flit.FwdTxnID() == 5);

    RSP response{};
    response.SrcID() = 6;
    response.TgtID() = 2;
    response.TxnID() = snoop.flit.TxnID();
    response.Opcode() = CHI::Eb::Opcodes::RSP::SnpRespFwded;
    response.Resp() = CHI::Eb::Resps::SnpResp_I;
    response.FwdState() = CHI::Eb::FwdStates::I;
    response.RespErr() = CHI::Eb::RespErrs::NDERR;
    LUNATIC_CHECK(driver.node.PushRXRSP(response));
    driver.Tick();
    CheckNDERRReadData(driver);
    driver.Quiesce();
    const LineKey line = LineKey::FromAddress(address, false);
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.InspectRequester(line, 6) == CacheState::I);
    LUNATIC_CHECK(driver.node.InspectRequester(line, 4) == CacheState::I);
}

LUNATIC_TEST(Stage9VictimErrorDirtyDataPropagatesToBackendWrite)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.policy = PolicyKind::Inclusive;
    options.slcSets = 1;
    options.slcWays = 1;
    TestDriver driver(options);
    const LineKey victim = LineKey::FromAddress(0x6b500, false);
    const LineKey allocation = LineKey::FromAddress(0x6b540, false);
    const LineData data = ErrorLineData();
    SeedDirtyOwner(driver, victim.address, data, true);

    LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 6, allocation.address)));
    const auto allocationResponse = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
    ByteEnables enables{};
    enables.fill(1);
    driver.WriteData(4, 6, allocationResponse.DBID(), data, enables, CHI::Eb::Opcodes::DAT::CopyBackWrData,
                     CHI::Eb::Resps::CopyBackWrData_UD_PD);
    const auto victimSnoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
    LUNATIC_CHECK(victimSnoop.target == 6);
    for (size_t beat = 0; beat < 2; ++beat)
    {
        DAT packet{};
        packet.SrcID() = 6;
        packet.TgtID() = 2;
        packet.TxnID() = victimSnoop.flit.TxnID();
        packet.Opcode() = CHI::Eb::Opcodes::DAT::SnpRespData;
        packet.Resp() = CHI::Eb::Resps::SnpRespData_I_PD;
        packet.RespErr() = beat == 0 ? CHI::Eb::RespErrs::DERR : CHI::Eb::RespErrs::OK;
        packet.DataID() = beat * 2;
        packet.CCID() = 0;
        SetPacketData(packet, data, beat);
        LUNATIC_CHECK(driver.node.PushRXDAT(packet));
    }
    driver.Tick();

    const auto downstreamWrite = driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    LUNATIC_CHECK(downstreamWrite.Addr() == victim.address);
    SendBackendResponse(driver, downstreamWrite, CHI::Eb::Opcodes::RSP::DBIDResp, CHI::Eb::RespErrs::OK);
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto writeData = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(writeData.RespErr() == (beat == 0 ? CHI::Eb::RespErrs::DERR : CHI::Eb::RespErrs::OK));
    }
    SendBackendResponse(driver, downstreamWrite, CHI::Eb::Opcodes::RSP::Comp, CHI::Eb::RespErrs::OK);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
    LUNATIC_CHECK(driver.node.InspectHome(allocation).valid);
}
