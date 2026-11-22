#include "sequence.hpp"

using namespace XactTest;

namespace {

    void CheckStashGroupResponse(uint16_t opcode)
    {
        RNFixture fixture;
        auto request = Request(Opcodes::REQ::StashOnceSepUnique);
        request.StashNIDValid() = 1;
        request.TxnID() = 0;
        request.StashNID() = 4;
        request.StashGroupID() = 0x12;
        XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, request));

        auto response = Response(2, 4, opcode, 0);
        response.StashGroupID() = 0x13;
        XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 2, response) != XactDenial::ACCEPTED);
    }

}

XACT_TEST(Stage5OracleStashDoneChecksGroup)
{
    CheckStashGroupResponse(Opcodes::RSP::StashDone);
}

XACT_TEST(Stage5OracleCompStashDoneChecksGroup)
{
    CheckStashGroupResponse(Opcodes::RSP::CompStashDone);
}

XACT_TEST(Stage5OracleSeparatedStashResponsesComplete)
{
    RNFixture fixture;
    auto request = Request(Opcodes::REQ::StashOnceSepShared);
    request.StashNIDValid() = 1;
    request.TxnID() = 0;
    request.StashNID() = 4;
    request.StashGroupID() = 0x24;
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1, request));

    auto done = Response(2, 4, Opcodes::RSP::StashDone, 0);
    done.StashGroupID() = 0x24;
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 2, done));
    XACT_CHECK(fixture.joint.HasInflight());

    const auto comp = Response(2, 4, Opcodes::RSP::Comp, 0);
    XACT_CHECK(fixture.joint.NextRXRSP(fixture.global, 3, comp) == XactDenial::ACCEPTED);
    XACT_CHECK(!fixture.joint.HasInflight());
}

namespace {

    Flits::SNP<config> DataPullSnoop(uint16_t opcode, uint16_t txnId = 0x30)
    {
        Flits::SNP<config> snoop{};
        snoop.SrcID() = 2;
        snoop.TxnID() = txnId;
        snoop.Opcode() = opcode;
        snoop.Addr() = 0x10000 >> 3;
        snoop.DoNotGoToSD() = 1;
        snoop.StashLPIDValid() = 1;
        snoop.StashLPID() = 3;
        return snoop;
    }

    void StartDataPullSnoop(RNFixture& fixture, uint16_t opcode, uint16_t txnId = 0x30)
    {
        XACT_ACCEPT(fixture.joint.NextRXSNP(fixture.global, 1, DataPullSnoop(opcode, txnId), 4));
    }

    Flits::RSP<config> DataPullResponse(uint8_t resp, uint16_t dbid = 0x37)
    {
        auto response = Response(4, 2, Opcodes::RSP::SnpResp, 0x30, dbid);
        response.Resp() = resp;
        response.DataPull() = 1;
        return response;
    }

    Flits::DAT<config> DataPullSnoopData(uint16_t opcode, uint8_t resp, uint8_t dataId,
                                         uint16_t dbid = 0x37)
    {
        auto data = Data(4, 2, opcode, 0x30, dataId, dbid);
        data.Resp() = resp;
        data.DataPull() = 1;
        if (opcode == Opcodes::DAT::SnpRespDataPtl)
            data.BE() = 0xffff;
        return data;
    }

    Flits::DAT<config> DataPullReadData(uint16_t source, uint16_t opcode, uint8_t dataId)
    {
        auto data = Data(source, 4, opcode, 0x37, dataId, 0x91);
        data.HomeNID() = 2;
        data.Resp() = opcode == Opcodes::DAT::DataSepResp
            ? Resps::DataSepResp_UC : Resps::CompData_UC;
        return data;
    }

    void SendDataPullCompAck(RNFixture& fixture)
    {
        XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 20,
            Response(4, 2, Opcodes::RSP::CompAck, 0x91)));
    }

    void FinishDataPullRead(RNFixture& fixture, uint16_t source = 2)
    {
        XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 10,
            DataPullReadData(source, Opcodes::DAT::CompData, 0)));
        XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 11,
            DataPullReadData(source, Opcodes::DAT::CompData, 2)));
        SendDataPullCompAck(fixture);
        XACT_CHECK(!fixture.joint.HasInflight());
    }

}

XACT_TEST(Stage5OracleDataPullStashRSP)
{
    const std::pair<uint16_t, uint8_t> cases[] = {
        { Opcodes::SNP::SnpUniqueStash, Resps::SnpResp_I },
        { Opcodes::SNP::SnpMakeInvalidStash, Resps::SnpResp_I },
        { Opcodes::SNP::SnpStashUnique, Resps::SnpResp_I },
        { Opcodes::SNP::SnpStashUnique, Resps::SnpResp_UC },
        { Opcodes::SNP::SnpStashUnique, Resps::SnpResp_SC },
        { Opcodes::SNP::SnpStashUnique, Resps::SnpResp_SD },
        { Opcodes::SNP::SnpStashShared, Resps::SnpResp_I },
        { Opcodes::SNP::SnpStashShared, Resps::SnpResp_UC }
    };

    for (const auto& [opcode, resp] : cases)
    {
        RNFixture fixture;
        StartDataPullSnoop(fixture, opcode);
        XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2, DataPullResponse(resp)));
        FinishDataPullRead(fixture);
    }
}

XACT_TEST(Stage5OracleDataPullUniqueStashData)
{
    const std::pair<uint16_t, uint8_t> cases[] = {
        { Opcodes::DAT::SnpRespData, Resps::SnpRespData_I_PD },
        { Opcodes::DAT::SnpRespDataPtl, Resps::SnpRespDataPtl_I_PD }
    };

    for (const auto& [opcode, resp] : cases)
    {
        RNFixture fixture;
        StartDataPullSnoop(fixture, Opcodes::SNP::SnpUniqueStash);
        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 2,
            DataPullSnoopData(opcode, resp, 0)));
        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 3,
            DataPullSnoopData(opcode, resp, 2)));
        FinishDataPullRead(fixture);
    }
}

XACT_TEST(Stage5OracleDataPullReadPrecedesFinalSnoopData)
{
    RNFixture fixture;
    StartDataPullSnoop(fixture, Opcodes::SNP::SnpUniqueStash);
    XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 2,
        DataPullSnoopData(Opcodes::DAT::SnpRespData, Resps::SnpRespData_I_PD, 0)));
    XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 3,
        DataPullReadData(2, Opcodes::DAT::CompData, 0)));
    XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 4,
        DataPullReadData(2, Opcodes::DAT::CompData, 2)));
    SendDataPullCompAck(fixture);
    XACT_CHECK(fixture.joint.HasInflight());
    XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 21,
        DataPullSnoopData(Opcodes::DAT::SnpRespData, Resps::SnpRespData_I_PD, 2)));
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage5OracleDataPullDMT)
{
    RNFixture fixture;
    fixture.global.TOPOLOGY.SetNode(6, Xact::NodeType::SN_F);
    StartDataPullSnoop(fixture, Opcodes::SNP::SnpStashShared);
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2,
        DataPullResponse(Resps::SnpResp_I)));
    FinishDataPullRead(fixture, 6);
}

XACT_TEST(Stage5OracleDataPullSeparatedResponse)
{
    RNFixture fixture;
    StartDataPullSnoop(fixture, Opcodes::SNP::SnpStashUnique);
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2,
        DataPullResponse(Resps::SnpResp_SC)));
    auto response = Response(2, 4, Opcodes::RSP::RespSepData, 0x37, 0x91);
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 3, response));
    SendDataPullCompAck(fixture);
    XACT_CHECK(fixture.joint.HasInflight());
    XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 21,
        DataPullReadData(2, Opcodes::DAT::DataSepResp, 0)));
    XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 22,
        DataPullReadData(2, Opcodes::DAT::DataSepResp, 2)));
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage5OracleDataPullClone)
{
    RNFixture fixture;
    StartDataPullSnoop(fixture, Opcodes::SNP::SnpStashShared);
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2,
        DataPullResponse(Resps::SnpResp_I)));
    fixture.joint.Fork();
    FinishDataPullRead(fixture);
}

XACT_TEST(Stage5OracleDataPullRejectsInvalidResponses)
{
    {
        RNFixture fixture;
        auto snoop = DataPullSnoop(Opcodes::SNP::SnpUnique);
        snoop.StashLPIDValid() = 0;
        snoop.StashLPID() = 0;
        XACT_ACCEPT(fixture.joint.NextRXSNP(fixture.global, 1, snoop, 4));
        XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 2,
            DataPullResponse(Resps::SnpResp_I)) == XactDenial::DENIED_RSP_FIELD_DATAPULL);
    }
    {
        RNFixture fixture;
        StartDataPullSnoop(fixture, Opcodes::SNP::SnpStashUnique);
        auto response = DataPullResponse(Resps::SnpResp_I);
        response.DataPull() = 2;
        XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 2, response) ==
            XactDenial::DENIED_RSP_FIELD_DATAPULL);
    }
    {
        RNFixture fixture;
        StartDataPullSnoop(fixture, Opcodes::SNP::SnpStashShared);
        XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 2,
            DataPullResponse(Resps::SnpResp_SC)) == XactDenial::DENIED_RSP_FIELD_DATAPULL);
    }
    {
        RNFixture fixture;
        StartDataPullSnoop(fixture, Opcodes::SNP::SnpMakeInvalidStash);
        XACT_CHECK(fixture.joint.NextTXDAT(fixture.global, 2,
            DataPullSnoopData(Opcodes::DAT::SnpRespData, Resps::SnpRespData_I_PD, 0)) ==
            XactDenial::DENIED_DAT_FIELD_DATAPULL);
    }
}

XACT_TEST(Stage5OracleDataPullChecksAllSnoopDataPackets)
{
    {
        RNFixture fixture;
        StartDataPullSnoop(fixture, Opcodes::SNP::SnpUniqueStash);
        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 2,
            DataPullSnoopData(Opcodes::DAT::SnpRespData, Resps::SnpRespData_I_PD, 0)));
        auto second = DataPullSnoopData(Opcodes::DAT::SnpRespData, Resps::SnpRespData_I_PD, 2);
        second.DataPull() = 0;
        XACT_CHECK(fixture.joint.NextTXDAT(fixture.global, 3, second) ==
            XactDenial::DENIED_DAT_FIELD_DATAPULL);
    }
    {
        RNFixture fixture;
        StartDataPullSnoop(fixture, Opcodes::SNP::SnpUniqueStash);
        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 2,
            DataPullSnoopData(Opcodes::DAT::SnpRespData, Resps::SnpRespData_I_PD, 0)));
        XACT_CHECK(fixture.joint.NextTXDAT(fixture.global, 3,
            DataPullSnoopData(Opcodes::DAT::SnpRespData, Resps::SnpRespData_I_PD, 2, 0x38)) ==
            XactDenial::DENIED_DAT_DBID_MISMATCH);
    }
}

XACT_TEST(Stage5OracleDataPullChecksDBIDUniqueness)
{
    {
        RNFixture fixture;
        XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1,
            Request(Opcodes::REQ::ReadUnique, 0x37)));
        StartDataPullSnoop(fixture, Opcodes::SNP::SnpStashUnique);
        XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 2,
            DataPullResponse(Resps::SnpResp_I)) == XactDenial::DENIED_RSP_DBID_IN_USE);
    }
    {
        RNFixture fixture;
        StartDataPullSnoop(fixture, Opcodes::SNP::SnpStashUnique);
        XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2,
            DataPullResponse(Resps::SnpResp_I)));
        StartDataPullSnoop(fixture, Opcodes::SNP::SnpStashShared, 0x31);
        auto response = DataPullResponse(Resps::SnpResp_I);
        response.TxnID() = 0x31;
        XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 3, response) ==
            XactDenial::DENIED_RSP_DBID_IN_USE);
    }
}

XACT_TEST(Stage5OracleDataPullRejectsEarlyCompAck)
{
    RNFixture fixture;
    StartDataPullSnoop(fixture, Opcodes::SNP::SnpStashUnique);
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2,
        DataPullResponse(Resps::SnpResp_I)));
    XACT_CHECK(fixture.joint.NextTXRSP(fixture.global, 3,
        Response(4, 2, Opcodes::RSP::CompAck, 0x91)) == XactDenial::DENIED_RSP_TXNID_NOT_EXIST);
}

XACT_TEST(Stage5OracleDataSourceIsNotDataPull)
{
    RNFixture fixture;
    auto snoop = DataPullSnoop(Opcodes::SNP::SnpUnique);
    snoop.StashLPIDValid() = 0;
    snoop.StashLPID() = 0;
    XACT_ACCEPT(fixture.joint.NextRXSNP(fixture.global, 1, snoop, 4));
    auto first = DataPullSnoopData(Opcodes::DAT::SnpRespData, Resps::SnpRespData_I_PD, 0);
    first.DataSource() = 1;
    XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 2, first));
    auto second = DataPullSnoopData(Opcodes::DAT::SnpRespData, Resps::SnpRespData_I_PD, 2);
    second.DataSource() = 8;
    XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 3, second));
    XACT_CHECK(!fixture.joint.HasInflight());
}
