#include "common.hpp"

#include "cchi/cohestra/cohestra_v3/interface_chi.hpp"

using namespace Cohestra;
using namespace Cohestra::details;

#define LUNATIC_TEST_CONTROLS(prefix) \
    uint8_t prefix##_flitpend = 0; \
    uint8_t prefix##_flitv = 0; \
    uint8_t prefix##_lcrdv = 0

#define LUNATIC_TEST_RSP_FIELDS(prefix) \
    uint64_t prefix##_QoS = 0; \
    uint64_t prefix##_TgtID = 0; \
    uint64_t prefix##_SrcID = 0; \
    uint64_t prefix##_TxnID = 0; \
    uint64_t prefix##_Opcode = 0; \
    uint64_t prefix##_RespErr = 0; \
    uint64_t prefix##_Resp = 0; \
    uint64_t prefix##_FwdState = 0; \
    uint64_t prefix##_DataPull = 0; \
    uint64_t prefix##_CBusy = 0; \
    uint64_t prefix##_DBID = 0; \
    uint64_t prefix##_PGroupID = 0; \
    uint64_t prefix##_StashGroupID = 0; \
    uint64_t prefix##_TagGroupID = 0; \
    uint64_t prefix##_PCrdType = 0; \
    uint64_t prefix##_TagOp = 0; \
    uint64_t prefix##_TraceTag = 0

#define LUNATIC_TEST_DAT_FIELDS(prefix) \
    uint64_t prefix##_QoS = 0; \
    uint64_t prefix##_TgtID = 0; \
    uint64_t prefix##_SrcID = 0; \
    uint64_t prefix##_TxnID = 0; \
    uint64_t prefix##_HomeNID = 0; \
    uint64_t prefix##_Opcode = 0; \
    uint64_t prefix##_RespErr = 0; \
    uint64_t prefix##_Resp = 0; \
    uint64_t prefix##_FwdState = 0; \
    uint64_t prefix##_DataPull = 0; \
    uint64_t prefix##_DataSource = 0; \
    uint64_t prefix##_CBusy = 0; \
    uint64_t prefix##_DBID = 0; \
    uint64_t prefix##_CCID = 0; \
    uint64_t prefix##_DataID = 0; \
    uint64_t prefix##_TagOp = 0; \
    uint64_t prefix##_Tag = 0; \
    uint64_t prefix##_TU = 0; \
    uint64_t prefix##_TraceTag = 0; \
    uint64_t prefix##_RSVDC = 0; \
    uint64_t prefix##_BE = 0; \
    uint64_t prefix##_Data[4] = {}; \
    uint64_t prefix##_DataCheck = 0; \
    uint64_t prefix##_Poison = 0

namespace {

    struct SeparatePinMock {
        LUNATIC_TEST_CONTROLS(chi_rn0_txrsp);
        LUNATIC_TEST_CONTROLS(chi_rn0_rxrsp);
        LUNATIC_TEST_CONTROLS(chi_rn0_txdat);
        LUNATIC_TEST_CONTROLS(chi_rn0_rxdat);
        LUNATIC_TEST_CONTROLS(chi_sn0_txdat);
        LUNATIC_TEST_CONTROLS(chi_sn0_rxdat);
        LUNATIC_TEST_CONTROLS(mon_chi_rn0_txrsp);
        LUNATIC_TEST_CONTROLS(mon_chi_rn0_rxrsp);
        LUNATIC_TEST_CONTROLS(mon_chi_rn0_txdat);
        LUNATIC_TEST_CONTROLS(mon_chi_rn0_rxdat);

        LUNATIC_TEST_RSP_FIELDS(chi_rn0_txrsp_bits);
        LUNATIC_TEST_RSP_FIELDS(chi_rn0_rxrsp_bits);
        LUNATIC_TEST_DAT_FIELDS(chi_rn0_txdat_bits);
        LUNATIC_TEST_DAT_FIELDS(chi_rn0_rxdat_bits);
        LUNATIC_TEST_DAT_FIELDS(chi_sn0_txdat_bits);
        LUNATIC_TEST_DAT_FIELDS(chi_sn0_rxdat_bits);
        LUNATIC_TEST_RSP_FIELDS(mon_chi_rn0_txrsp_flit);
        LUNATIC_TEST_RSP_FIELDS(mon_chi_rn0_rxrsp_flit);
        LUNATIC_TEST_DAT_FIELDS(mon_chi_rn0_txdat_flit);
        LUNATIC_TEST_DAT_FIELDS(mon_chi_rn0_rxdat_flit);
    };

    Cohestra::CHI::RSPFlit MakeResponse()
    {
        Cohestra::CHI::RSPFlit flit{};
        flit.TgtID() = 3;
        flit.SrcID() = 2;
        flit.TxnID() = 0x55;
        flit.DBID() = 0x2a;
        flit.Opcode() = ::CHI::Eb::Opcodes::RSP::CompDBIDResp;
        flit.Resp() = 3;
        flit.TraceTag() = 1;
        return flit;
    }

    Cohestra::CHI::DATFlit MakeData()
    {
        Cohestra::CHI::DATFlit flit{};
        flit.TgtID() = 3;
        flit.SrcID() = 2;
        flit.HomeNID() = 1;
        flit.TxnID() = 0x66;
        flit.DBID() = 0x2b;
        flit.Opcode() = ::CHI::Eb::Opcodes::DAT::CompData;
        flit.DataID() = 2;
        flit.BE() = 0xffffffffULL;
        flit.Data()[0] = 0x0123456789abcdefULL;
        flit.Data()[3] = 0xfedcba9876543210ULL;
        return flit;
    }

}

LUNATIC_TEST(Stage9SeparateFieldRXPinMapping)
{
    SeparatePinMock mock;
    const auto response = MakeResponse();
    const auto data = MakeData();

    using RNResponse = V3CHISeparateFieldRNChannelPolicy<SeparatePinMock, 0, RXRSPChannel>;
    RNResponse::DriveFlit(mock, &response, true);
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_bits_TgtID == response.TgtID());
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_bits_TxnID == response.TxnID());
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_bits_DBID == response.DBID());
    LUNATIC_CHECK(mock.chi_rn0_txrsp_bits_TgtID == 0);
    LUNATIC_CHECK(mock.chi_rn0_txrsp_bits_TxnID == 0);
    LUNATIC_CHECK(mock.chi_rn0_txrsp_bits_DBID == 0);

    Cohestra::CHI::RSPFlit sampledResponse{};
    LUNATIC_CHECK(RNResponse::SampleFlit(mock, &sampledResponse));
    LUNATIC_CHECK(sampledResponse.TgtID() == response.TgtID());
    LUNATIC_CHECK(sampledResponse.TxnID() == response.TxnID());
    LUNATIC_CHECK(sampledResponse.DBID() == response.DBID());

    using RNData = V3CHISeparateFieldRNChannelPolicy<SeparatePinMock, 0, RXDATChannel>;
    RNData::DriveFlit(mock, &data, true);
    LUNATIC_CHECK(mock.chi_rn0_rxdat_bits_HomeNID == data.HomeNID());
    LUNATIC_CHECK(mock.chi_rn0_rxdat_bits_TxnID == data.TxnID());
    LUNATIC_CHECK(mock.chi_rn0_rxdat_bits_Data[0] == data.Data()[0]);
    LUNATIC_CHECK(mock.chi_rn0_rxdat_bits_Data[3] == data.Data()[3]);
    LUNATIC_CHECK(mock.chi_rn0_txdat_bits_HomeNID == 0);
    LUNATIC_CHECK(mock.chi_rn0_txdat_bits_TxnID == 0);
    LUNATIC_CHECK(mock.chi_rn0_txdat_bits_Data[0] == 0);

    Cohestra::CHI::DATFlit sampledData{};
    LUNATIC_CHECK(RNData::SampleFlit(mock, &sampledData));
    LUNATIC_CHECK(sampledData.HomeNID() == data.HomeNID());
    LUNATIC_CHECK(sampledData.TxnID() == data.TxnID());
    LUNATIC_CHECK(sampledData.Data()[0] == data.Data()[0]);
    LUNATIC_CHECK(sampledData.Data()[3] == data.Data()[3]);

    using SNData = V3CHISeparateFieldSNChannelPolicy<SeparatePinMock, 0, RXDATChannel>;
    SNData::DriveFlit(mock, &data, true);
    LUNATIC_CHECK(mock.chi_sn0_rxdat_bits_HomeNID == data.HomeNID());
    LUNATIC_CHECK(mock.chi_sn0_rxdat_bits_TxnID == data.TxnID());
    LUNATIC_CHECK(mock.chi_sn0_rxdat_bits_Data[0] == data.Data()[0]);
    LUNATIC_CHECK(mock.chi_sn0_txdat_bits_HomeNID == 0);
    LUNATIC_CHECK(mock.chi_sn0_txdat_bits_TxnID == 0);
    LUNATIC_CHECK(mock.chi_sn0_txdat_bits_Data[0] == 0);

    sampledData = {};
    LUNATIC_CHECK(SNData::SampleFlit(mock, &sampledData));
    LUNATIC_CHECK(sampledData.HomeNID() == data.HomeNID());
    LUNATIC_CHECK(sampledData.TxnID() == data.TxnID());
    LUNATIC_CHECK(sampledData.Data()[0] == data.Data()[0]);

    mock.mon_chi_rn0_rxrsp_flitv = 1;
    mock.mon_chi_rn0_rxrsp_flit_QoS = response.QoS();
    mock.mon_chi_rn0_rxrsp_flit_TgtID = response.TgtID();
    mock.mon_chi_rn0_rxrsp_flit_SrcID = response.SrcID();
    mock.mon_chi_rn0_rxrsp_flit_TxnID = response.TxnID();
    mock.mon_chi_rn0_rxrsp_flit_Opcode = response.Opcode();
    mock.mon_chi_rn0_rxrsp_flit_RespErr = response.RespErr();
    mock.mon_chi_rn0_rxrsp_flit_Resp = response.Resp();
    mock.mon_chi_rn0_rxrsp_flit_FwdState = response.FwdState();
    mock.mon_chi_rn0_rxrsp_flit_DataPull = response.DataPull();
    mock.mon_chi_rn0_rxrsp_flit_CBusy = response.CBusy();
    mock.mon_chi_rn0_rxrsp_flit_DBID = response.DBID();
    mock.mon_chi_rn0_rxrsp_flit_PGroupID = response.PGroupID();
    mock.mon_chi_rn0_rxrsp_flit_StashGroupID = response.StashGroupID();
    mock.mon_chi_rn0_rxrsp_flit_TagGroupID = response.TagGroupID();
    mock.mon_chi_rn0_rxrsp_flit_PCrdType = response.PCrdType();
    mock.mon_chi_rn0_rxrsp_flit_TagOp = response.TagOp();
    mock.mon_chi_rn0_rxrsp_flit_TraceTag = response.TraceTag();
    mock.mon_chi_rn0_txrsp_flit_TgtID = 0x7f;
    mock.mon_chi_rn0_txrsp_flit_TxnID = 0x7f;
    mock.mon_chi_rn0_txrsp_flit_DBID = 0x7f;
    using MonitorResponse = V3CHISeparateFieldMonitorRNChannelPolicy<SeparatePinMock, 0, RXRSPChannel>;
    sampledResponse = {};
    LUNATIC_CHECK(MonitorResponse::SampleFlit(mock, &sampledResponse));
    LUNATIC_CHECK(sampledResponse.TgtID() == response.TgtID());
    LUNATIC_CHECK(sampledResponse.TxnID() == response.TxnID());
    LUNATIC_CHECK(sampledResponse.DBID() == response.DBID());

    mock.mon_chi_rn0_rxdat_flitv = 1;
    mock.mon_chi_rn0_rxdat_flit_HomeNID = data.HomeNID();
    mock.mon_chi_rn0_rxdat_flit_TxnID = data.TxnID();
    mock.mon_chi_rn0_rxdat_flit_Data[0] = data.Data()[0];
    mock.mon_chi_rn0_rxdat_flit_Data[3] = data.Data()[3];
    mock.mon_chi_rn0_txdat_flit_HomeNID = 0x7f;
    mock.mon_chi_rn0_txdat_flit_TxnID = 0x7f;
    mock.mon_chi_rn0_txdat_flit_Data[0] = 0x7f;
    using MonitorData = V3CHISeparateFieldMonitorRNChannelPolicy<SeparatePinMock, 0, RXDATChannel>;
    sampledData = {};
    LUNATIC_CHECK(MonitorData::SampleFlit(mock, &sampledData));
    LUNATIC_CHECK(sampledData.HomeNID() == data.HomeNID());
    LUNATIC_CHECK(sampledData.TxnID() == data.TxnID());
    LUNATIC_CHECK(sampledData.Data()[0] == data.Data()[0]);
    LUNATIC_CHECK(sampledData.Data()[3] == data.Data()[3]);
}
