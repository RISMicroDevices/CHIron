#include "common.hpp"
#include "cchi/cohestra/cohestra_v3/interface_chi.hpp"
#include "cchi/cohestra/cohestra_lunatic_link.hpp"

using namespace LunaticTest;

#define LUNATIC_LINK_CONTROLS(prefix) \
    uint8_t prefix##_flitpend = 0; \
    uint8_t prefix##_flitv = 0; \
    uint8_t prefix##_lcrdv = 0

#define LUNATIC_LINK_REQ_FIELDS(prefix) \
    uint64_t prefix##_QoS = 0; \
    uint64_t prefix##_TgtID = 0; \
    uint64_t prefix##_SrcID = 0; \
    uint64_t prefix##_TxnID = 0; \
    uint64_t prefix##_ReturnNID = 0; \
    uint64_t prefix##_StashNID = 0; \
    uint64_t prefix##_StashNIDValid = 0; \
    uint64_t prefix##_Endian = 0; \
    uint64_t prefix##_ReturnTxnID = 0; \
    uint64_t prefix##_StashLPIDValid = 0; \
    uint64_t prefix##_StashLPID = 0; \
    uint64_t prefix##_Opcode = 0; \
    uint64_t prefix##_Size = 0; \
    uint64_t prefix##_Addr = 0; \
    uint64_t prefix##_NS = 0; \
    uint64_t prefix##_LikelyShared = 0; \
    uint64_t prefix##_AllowRetry = 0; \
    uint64_t prefix##_Order = 0; \
    uint64_t prefix##_PCrdType = 0; \
    uint64_t prefix##_MemAttr = 0; \
    uint64_t prefix##_SnpAttr = 0; \
    uint64_t prefix##_LPID = 0; \
    uint64_t prefix##_Excl = 0; \
    uint64_t prefix##_SnoopMe = 0; \
    uint64_t prefix##_ExpCompAck = 0; \
    uint64_t prefix##_TraceTag = 0; \
    uint64_t prefix##_SLCRepHint = 0; \
    uint64_t prefix##_Deep = 0; \
    uint64_t prefix##_DoDWT = 0; \
    uint64_t prefix##_PGroupID = 0; \
    uint64_t prefix##_StashGroupID = 0; \
    uint64_t prefix##_TagGroupID = 0; \
    uint64_t prefix##_TagOp = 0; \
    uint64_t prefix##_MPAM = 0; \
    uint64_t prefix##_RSVDC = 0

#define LUNATIC_LINK_RSP_FIELDS(prefix) \
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

#define LUNATIC_LINK_DAT_FIELDS(prefix) \
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

#define LUNATIC_LINK_SNP_FIELDS(prefix) \
    uint64_t prefix##_QoS = 0; \
    uint64_t prefix##_SrcID = 0; \
    uint64_t prefix##_TxnID = 0; \
    uint64_t prefix##_FwdNID = 0; \
    uint64_t prefix##_FwdTxnID = 0; \
    uint64_t prefix##_StashLPID = 0; \
    uint64_t prefix##_StashLPIDValid = 0; \
    uint64_t prefix##_VMIDExt = 0; \
    uint64_t prefix##_Opcode = 0; \
    uint64_t prefix##_Addr = 0; \
    uint64_t prefix##_NS = 0; \
    uint64_t prefix##_DoNotGoToSD = 0; \
    uint64_t prefix##_RetToSrc = 0; \
    uint64_t prefix##_TraceTag = 0; \
    uint64_t prefix##_MPAM = 0

namespace {

    struct LinkMock {
        LUNATIC_LINK_CONTROLS(chi_rn0_txreq);
        LUNATIC_LINK_CONTROLS(chi_rn0_txrsp);
        LUNATIC_LINK_CONTROLS(chi_rn0_txdat);
        LUNATIC_LINK_CONTROLS(chi_rn0_rxrsp);
        LUNATIC_LINK_CONTROLS(chi_rn0_rxdat);
        LUNATIC_LINK_CONTROLS(chi_rn0_rxsnp);
        LUNATIC_LINK_REQ_FIELDS(chi_rn0_txreq_bits);
        LUNATIC_LINK_RSP_FIELDS(chi_rn0_txrsp_bits);
        LUNATIC_LINK_DAT_FIELDS(chi_rn0_txdat_bits);
        LUNATIC_LINK_RSP_FIELDS(chi_rn0_rxrsp_bits);
        LUNATIC_LINK_DAT_FIELDS(chi_rn0_rxdat_bits);
        LUNATIC_LINK_SNP_FIELDS(chi_rn0_rxsnp_bits);

        uint32_t chi_rn0_txreq_bits[(Cohestra::CHI::REQFlit::WIDTH + 31) / 32] = {};
        uint32_t chi_rn0_txrsp_bits[(Cohestra::CHI::RSPFlit::WIDTH + 31) / 32] = {};
        uint32_t chi_rn0_txdat_bits[(Cohestra::CHI::DATFlit::WIDTH + 31) / 32] = {};
        uint32_t chi_rn0_rxrsp_bits[(Cohestra::CHI::RSPFlit::WIDTH + 31) / 32] = {};
        uint32_t chi_rn0_rxdat_bits[(Cohestra::CHI::DATFlit::WIDTH + 31) / 32] = {};
        uint32_t chi_rn0_rxsnp_bits[(Cohestra::CHI::SNPFlit::WIDTH + 31) / 32] = {};

        uint8_t chi_rn0_txlinkactivereq = 0;
        uint8_t chi_rn0_txlinkactiveack = 0;
        uint8_t chi_rn0_rxlinkactivereq = 0;
        uint8_t chi_rn0_rxlinkactiveack = 0;
        uint8_t chi_rn0_rxsactive = 0;
        uint8_t chi_rn0_syscoack = 0;
    };

    void ExpectNoCreditsOrPayload(const LinkMock& mock)
    {
        LUNATIC_CHECK(!mock.chi_rn0_txreq_lcrdv);
        LUNATIC_CHECK(!mock.chi_rn0_txrsp_lcrdv);
        LUNATIC_CHECK(!mock.chi_rn0_txdat_lcrdv);
        LUNATIC_CHECK(!mock.chi_rn0_rxrsp_flitv);
        LUNATIC_CHECK(!mock.chi_rn0_rxdat_flitv);
        LUNATIC_CHECK(!mock.chi_rn0_rxsnp_flitv);
    }

}

LUNATIC_TEST(Stage9LunaticDirectionalLinkGating)
{
    LinkMock mock;
    Cohestra::LunaticCHIToRNInterface<LinkMock> transport(&mock);
    Cohestra::CHI::RSPFlit response{};
    response.SrcID() = 0;
    response.TgtID() = 1;
    response.TxnID() = 3;
    response.Opcode() = ::CHI::Eb::Opcodes::RSP::Comp;
    LUNATIC_CHECK(transport.PushRXRSP(0, response));

    transport.TickPreHandshake(0);
    ExpectNoCreditsOrPayload(mock);

    mock.chi_rn0_rxlinkactivereq = 1;
    mock.chi_rn0_txlinkactiveack = 1;
    transport.TickPreHandshake(1);
    LUNATIC_CHECK(mock.chi_rn0_txreq_lcrdv);
    LUNATIC_CHECK(mock.chi_rn0_txrsp_lcrdv);
    LUNATIC_CHECK(mock.chi_rn0_txdat_lcrdv);
    LUNATIC_CHECK(!mock.chi_rn0_rxrsp_flitv);
    LUNATIC_CHECK(!mock.chi_rn0_rxdat_flitv);
    LUNATIC_CHECK(!mock.chi_rn0_rxsnp_flitv);

    mock.chi_rn0_txlinkactivereq = 1;
    mock.chi_rn0_rxlinkactiveack = 1;
    mock.chi_rn0_rxrsp_lcrdv = 1;
    transport.TickPostHandshake(2);
    transport.TickPreHandshake(3);
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_flitv);
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_bits_TgtID == 1);
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_bits_TxnID == 3);

    mock.chi_rn0_rxlinkactivereq = 0;
    mock.chi_rn0_txlinkactiveack = 0;
    transport.TickPreHandshake(4);
    ExpectNoCreditsOrPayload(mock);
}

LUNATIC_TEST(Stage9LunaticTransmitDoesNotRequireReceiveRun)
{
    LinkMock mock;
    Cohestra::LunaticCHIToRNInterface<LinkMock> transport(&mock);
    Cohestra::CHI::RSPFlit response{};
    response.SrcID() = 0;
    response.TgtID() = 1;
    response.TxnID() = 4;
    response.Opcode() = ::CHI::Eb::Opcodes::RSP::Comp;
    LUNATIC_CHECK(transport.PushRXRSP(0, response));

    mock.chi_rn0_txlinkactivereq = 1;
    mock.chi_rn0_rxlinkactiveack = 1;
    mock.chi_rn0_rxrsp_lcrdv = 1;
    transport.TickPostHandshake(0);
    transport.TickPreHandshake(1);
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_flitv);
    LUNATIC_CHECK(!mock.chi_rn0_txreq_lcrdv);
    LUNATIC_CHECK(!mock.chi_rn0_txrsp_lcrdv);
    LUNATIC_CHECK(!mock.chi_rn0_txdat_lcrdv);
}
