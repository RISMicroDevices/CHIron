#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using RSP = TestDriver::RSP;
    using DAT = TestDriver::DAT;

    Options BackendOptions()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        return options;
    }

    RSP MakeResponse(NodeId source, NodeId target, TransactionId txnId,
                     uint16_t opcode, DataBufferId dbid = 0, uint8_t group = 0)
    {
        RSP response{};
        response.SrcID() = source;
        response.TgtID() = target;
        response.TxnID() = txnId;
        response.DBID() = dbid;
        response.Opcode() = opcode;
        response.Resp() = opcode == CHI::Eb::Opcodes::RSP::Comp ? uint8_t(CHI::Eb::Resps::Comp_I) : uint8_t(0);
        if (group)
            response.PGroupID() = group;
        return response;
    }

    void SendResponse(TestDriver& driver, const RSP& response)
    {
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    void SendBackendData(TestDriver& driver, const TestDriver::REQ& request, bool separated = false)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 2;
            data.HomeNID() = 2;
            data.TxnID() = request.TxnID();
            data.DBID() = request.TxnID();
            data.Opcode() = separated ? CHI::Eb::Opcodes::DAT::DataSepResp : CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = separated ? CHI::Eb::Resps::DataSepResp_UC : CHI::Eb::Resps::CompData_I;
            data.DataID() = beat * 2;
            data.CCID() = (request.Addr() >> 4) & 3;
            data.BE() = ~uint64_t(0);
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
    }

    LineData QoSPattern()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(0x27 + index * 15);
        return data;
    }

    ByteEnables QoSEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    void SeedDirtyProvider(TestDriver& driver, const LineKey& line, const LineData& data)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 90, line.address)));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 90, response.DBID(), data, QoSEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
            CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(6, 91, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        LUNATIC_CHECK(driver.node.InspectRequester(line, 6) == CacheState::UD);
    }

}

LUNATIC_TEST(Stage9QoSSeparatedReadFamily)
{
        Options options = BackendOptions();
        options.separatedReadResponse = true;
        options.separatedBackendRead = true;
        TestDriver driver(options);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 30, 0x73000, true);
        request.QoS() = 15;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnpSep);
        LUNATIC_CHECK(downstream.QoS() == 15);
        SendBackendData(driver, downstream, true);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData).QoS() == 15);
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::DataSepResp);
            LUNATIC_CHECK(data.QoS() == 15);
            dbid = data.DBID();
        }
        driver.CompAck(4, dbid);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage9QoSDCTFamily)
{
    {
        Options options;
        options.dctEnabled = true;
        TestDriver driver(options);
        const LineKey line = LineKey::FromAddress(0x73040, false);
        const LineData expected = QoSPattern();
        SeedDirtyProvider(driver, line, expected);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 31, line.address, true);
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        request.QoS() = 15;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpSharedFwd);
        LUNATIC_CHECK(snoop.flit.QoS() == 15);

        RSP response = MakeResponse(6, 2, snoop.flit.TxnID(), CHI::Eb::Opcodes::RSP::SnpRespFwded);
        response.Resp() = CHI::Eb::Resps::SnpResp_I;
        response.FwdState() = CHI::Eb::Resps::CompData_SD_PD;
        SendResponse(driver, response);
        LUNATIC_CHECK(!driver.node.PeekTXDAT());
        driver.CompAck(4, snoop.flit.TxnID());
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }
}

LUNATIC_TEST(Stage9QoSAtomicStashAndDirectPersistFamilies)
{
    {
        TestDriver driver(BackendOptions());
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 4, 32, 0x73100);
        request.Size() = 2;
        request.QoS() = 15;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto requesterDBID = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
        LUNATIC_CHECK(requesterDBID.QoS() == 15);

        DAT operand{};
        operand.SrcID() = 4;
        operand.TgtID() = 2;
        operand.TxnID() = requesterDBID.DBID();
        operand.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        operand.DataID() = 0;
        operand.CCID() = 0;
        operand.BE() = 0xf;
        operand.Data()[0] = 0x01020304;
        LUNATIC_CHECK(driver.node.PushRXDAT(operand));
        driver.Tick();

        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::AtomicStore::ADD);
        LUNATIC_CHECK(downstream.QoS() == 15);
        SendResponse(driver, MakeResponse(6, 2, downstream.TxnID(), CHI::Eb::Opcodes::RSP::DBIDResp, 77));
        LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData).QoS() == 15);
        SendResponse(driver, MakeResponse(6, 2, downstream.TxnID(), CHI::Eb::Opcodes::RSP::Comp, 77));
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).QoS() == 15);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    {
        TestDriver driver;
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteUniqueFullStash, 4, 33, 0x73140);
        request.ReturnNID() = 5;
        request.StashNIDValid() = 1;
        request.StashLPIDValid() = 1;
        request.StashLPID() = 3;
        request.QoS() = 15;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash);
        LUNATIC_CHECK(snoop.flit.QoS() == 15);
        SendResponse(driver, MakeResponse(5, 2, snoop.flit.TxnID(), CHI::Eb::Opcodes::RSP::SnpResp));
        const auto dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
        LUNATIC_CHECK(dbid.QoS() == 15);
        driver.WriteData(4, 33, dbid.DBID(), QoSPattern(), QoSEnables(), CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).QoS() == 15);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    {
        Options options = BackendOptions();
        options.directPersistEnabled = true;
        TestDriver driver(options);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 4, 34, 0x73180);
        request.PGroupID() = 0x68;
        request.Deep() = 1;
        request.QoS() = 15;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
        LUNATIC_CHECK(downstream.QoS() == 15 && downstream.ReturnNID() == 4);
        SendResponse(driver, MakeResponse(6, 2, downstream.TxnID(), CHI::Eb::Opcodes::RSP::Comp, 0, 0x68));
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).QoS() == 15);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }
}

LUNATIC_TEST(Stage9QoSDMTAndDWTFamilies)
{
    {
        Options options = BackendOptions();
        options.dmtEnabled = true;
        TestDriver driver(options);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 35, 0x73200, true);
        request.Order() = 2;
        request.QoS() = 15;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        LUNATIC_CHECK(downstream.QoS() == 15 && downstream.ReturnNID() == 4);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::ReadReceipt).QoS() == 15);
        SendResponse(driver, MakeResponse(6, 2, downstream.TxnID(), CHI::Eb::Opcodes::RSP::ReadReceipt));
        driver.CompAck(4, downstream.TxnID());
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    {
        Options options = BackendOptions();
        options.dwtEnabled = true;
        TestDriver driver(options);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteUniqueFull, 4, 36, 0x73240);
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        request.QoS() = 15;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
        LUNATIC_CHECK(downstream.QoS() == 15 && downstream.DoDWT() == 1 && downstream.ReturnNID() == 4);
        SendResponse(driver, MakeResponse(6, 2, downstream.TxnID(), CHI::Eb::Opcodes::RSP::Comp, 77));
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).QoS() == 15);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }
}
