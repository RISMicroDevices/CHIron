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

    mock.chi_rn0_txlinkactivereq = 1;
    mock.chi_rn0_txlinkactiveack = 1;
    transport.TickPreHandshake(1);
    LUNATIC_CHECK(mock.chi_rn0_txreq_lcrdv);
    LUNATIC_CHECK(mock.chi_rn0_txrsp_lcrdv);
    LUNATIC_CHECK(mock.chi_rn0_txdat_lcrdv);
    LUNATIC_CHECK(!mock.chi_rn0_rxrsp_flitv);
    LUNATIC_CHECK(!mock.chi_rn0_rxdat_flitv);
    LUNATIC_CHECK(!mock.chi_rn0_rxsnp_flitv);

    mock.chi_rn0_rxlinkactivereq = 1;
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

    mock.chi_rn0_rxlinkactivereq = 1;
    mock.chi_rn0_rxlinkactiveack = 1;
    mock.chi_rn0_rxrsp_lcrdv = 1;
    transport.TickPostHandshake(0);
    transport.TickPreHandshake(1);
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_flitv);
    LUNATIC_CHECK(!mock.chi_rn0_txreq_lcrdv);
    LUNATIC_CHECK(!mock.chi_rn0_txrsp_lcrdv);
    LUNATIC_CHECK(!mock.chi_rn0_txdat_lcrdv);
}

LUNATIC_TEST(Stage9LunaticReceiveRunUsesTxPair)
{
    LinkMock mock;
    Cohestra::LunaticCHIToRNInterface<LinkMock> transport(&mock);
    mock.chi_rn0_txlinkactivereq = 1;
    mock.chi_rn0_txlinkactiveack = 1;
    mock.chi_rn0_rxlinkactivereq = 0;
    mock.chi_rn0_rxlinkactiveack = 0;
    transport.TickPreHandshake(0);
    LUNATIC_CHECK(mock.chi_rn0_txreq_lcrdv);
    LUNATIC_CHECK(mock.chi_rn0_txrsp_lcrdv);
    LUNATIC_CHECK(mock.chi_rn0_txdat_lcrdv);
}

LUNATIC_TEST(Stage9LunaticTransmitRunUsesRxPair)
{
    LinkMock mock;
    Cohestra::LunaticCHIToRNInterface<LinkMock> transport(&mock);
    Cohestra::CHI::RSPFlit response{};
    response.SrcID() = 0;
    response.TgtID() = 1;
    response.TxnID() = 5;
    response.Opcode() = ::CHI::Eb::Opcodes::RSP::Comp;
    LUNATIC_CHECK(transport.PushRXRSP(0, response));
    mock.chi_rn0_txlinkactivereq = 0;
    mock.chi_rn0_txlinkactiveack = 0;
    mock.chi_rn0_rxlinkactivereq = 1;
    mock.chi_rn0_rxlinkactiveack = 1;
    mock.chi_rn0_rxrsp_lcrdv = 1;
    transport.TickPostHandshake(0);
    transport.TickPreHandshake(1);
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_flitv);
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_bits_TgtID == 1);
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_bits_TxnID == 5);
}

LUNATIC_TEST(Stage9LunaticDeactivationWaitsForRemoteRequest)
{
    LinkMock mock;
    Cohestra::LunaticCHIToRNInterface<LinkMock> transport(&mock);
    Cohestra::LunaticCHILinkControl<LinkMock> link(&mock, &transport);
    mock.chi_rn0_txlinkactivereq = 1;
    mock.chi_rn0_rxlinkactiveack = 1;
    link.SetActive(true);
    link.TickPreHandshake();
    LUNATIC_CHECK(mock.chi_rn0_rxlinkactivereq);
    LUNATIC_CHECK(mock.chi_rn0_txlinkactiveack);

    link.SetActive(false);
    link.TickPreHandshake();
    LUNATIC_CHECK(!mock.chi_rn0_rxlinkactivereq);
    LUNATIC_CHECK(mock.chi_rn0_txlinkactiveack);
}

namespace {

    using LinkTransport = Cohestra::LunaticCHIToRNInterface<LinkMock>;
    using LinkControl = Cohestra::LunaticCHILinkControl<LinkMock>;

    void TickLink(LinkMock& mock, LinkTransport& transport, LinkControl& link, uint64_t time)
    {
        (void)mock;
        link.TickPreHandshake();
        transport.TickPreHandshake(time);
        transport.TickPostHandshake(time);
    }

    void GrantRemoteCredits(LinkMock& mock, bool value)
    {
        mock.chi_rn0_rxrsp_lcrdv = value;
        mock.chi_rn0_rxdat_lcrdv = value;
        mock.chi_rn0_rxsnp_lcrdv = value;
    }

    void DriveRemoteCreditReturns(LinkMock& mock, bool value)
    {
        mock.chi_rn0_txreq_flitv = value;
        mock.chi_rn0_txrsp_flitv = value;
        mock.chi_rn0_txdat_flitv = value;
        mock.chi_rn0_txreq_bits_Opcode = 0;
        mock.chi_rn0_txreq_bits_TxnID = 0;
        mock.chi_rn0_txrsp_bits_Opcode = 0;
        mock.chi_rn0_txrsp_bits_TxnID = 0;
        mock.chi_rn0_txdat_bits_Opcode = 0;
        mock.chi_rn0_txdat_bits_TxnID = 0;
    }

    bool LocalCreditsReturned(const LinkTransport& transport)
    {
        const auto status = transport.GetPortStatus(0);
        for (size_t index = 0; index < 3; ++index)
            if (status[index].txCredits || status[index].txQueue || status[index].txDriven)
                return false;
        return true;
    }

    bool RemoteCreditsReturned(const LinkTransport& transport)
    {
        const auto status = transport.GetPortStatus(0);
        for (size_t index = 3; index < status.size(); ++index)
            if (status[index].rxCredits || status[index].rxQueue)
                return false;
        return true;
    }

    void ActivateLink(LinkMock& mock, LinkTransport& transport, LinkControl& link, uint64_t& time)
    {
        mock.chi_rn0_txlinkactivereq = 1;
        mock.chi_rn0_rxlinkactiveack = 1;
        link.SetActive(true);
        GrantRemoteCredits(mock, true);
        for (size_t cycle = 0; cycle < 4; ++cycle)
            TickLink(mock, transport, link, time++);
        GrantRemoteCredits(mock, false);
        LUNATIC_CHECK(mock.chi_rn0_rxlinkactivereq);
        LUNATIC_CHECK(mock.chi_rn0_txlinkactiveack);
        LUNATIC_CHECK(link.IsRun(0));
    }

}

LUNATIC_TEST(Stage9LunaticFullCreditReturnDeactivation)
{
    LinkMock mock;
    LinkTransport transport(&mock);
    LinkControl link(&mock, &transport);
    uint64_t time = 0;
    ActivateLink(mock, transport, link, time);

    Cohestra::CHI::RSPFlit response{};
    response.SrcID() = 0;
    response.TgtID() = 1;
    response.TxnID() = 6;
    response.Opcode() = ::CHI::Eb::Opcodes::RSP::Comp;
    LUNATIC_CHECK(transport.PushRXRSP(0, response));
    link.SetActive(false);
    TickLink(mock, transport, link, time++);
    LUNATIC_CHECK(mock.chi_rn0_rxlinkactivereq);
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_flitv);
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_bits_Opcode == ::CHI::Eb::Opcodes::RSP::Comp);

    for (size_t cycle = 0; cycle < 8 && !LocalCreditsReturned(transport); ++cycle)
    {
        TickLink(mock, transport, link, time++);
        LUNATIC_CHECK(!mock.chi_rn0_rxlinkactivereq);
        LUNATIC_CHECK(mock.chi_rn0_txlinkactiveack);
        if (mock.chi_rn0_rxrsp_flitv)
        {
            LUNATIC_CHECK(mock.chi_rn0_rxrsp_bits_Opcode == 0);
            LUNATIC_CHECK(mock.chi_rn0_rxrsp_bits_TxnID == 0);
        }
        if (mock.chi_rn0_rxdat_flitv)
        {
            LUNATIC_CHECK(mock.chi_rn0_rxdat_bits_Opcode == 0);
            LUNATIC_CHECK(mock.chi_rn0_rxdat_bits_TxnID == 0);
        }
        if (mock.chi_rn0_rxsnp_flitv)
        {
            LUNATIC_CHECK(mock.chi_rn0_rxsnp_bits_Opcode == 0);
            LUNATIC_CHECK(mock.chi_rn0_rxsnp_bits_TxnID == 0);
        }
    }
    LUNATIC_CHECK(LocalCreditsReturned(transport));
    LUNATIC_CHECK(transport.ValidateCreditAccounting());

    mock.chi_rn0_rxlinkactiveack = 0;
    mock.chi_rn0_txlinkactivereq = 0;
    DriveRemoteCreditReturns(mock, true);
    for (size_t cycle = 0; cycle < 8 && !RemoteCreditsReturned(transport); ++cycle)
    {
        TickLink(mock, transport, link, time++);
        LUNATIC_CHECK(mock.chi_rn0_txlinkactiveack);
    }
    LUNATIC_CHECK(RemoteCreditsReturned(transport));
    DriveRemoteCreditReturns(mock, false);
    TickLink(mock, transport, link, time++);
    LUNATIC_CHECK(!mock.chi_rn0_txlinkactiveack);
    LUNATIC_CHECK(!mock.chi_rn0_rxlinkactivereq);
    LUNATIC_CHECK(!mock.chi_rn0_rxsactive);
    LUNATIC_CHECK(transport.ValidateCreditAccounting());
}

LUNATIC_TEST(Stage9LunaticDeactivationReactivation)
{
    LinkMock mock;
    LinkTransport transport(&mock);
    LinkControl link(&mock, &transport);
    uint64_t time = 0;
    ActivateLink(mock, transport, link, time);

    link.SetActive(false);
    for (size_t cycle = 0; cycle < 8 && !LocalCreditsReturned(transport); ++cycle)
        TickLink(mock, transport, link, time++);
    LUNATIC_CHECK(LocalCreditsReturned(transport));
    mock.chi_rn0_rxlinkactiveack = 0;
    mock.chi_rn0_txlinkactivereq = 0;
    DriveRemoteCreditReturns(mock, true);
    for (size_t cycle = 0; cycle < 8 && !RemoteCreditsReturned(transport); ++cycle)
        TickLink(mock, transport, link, time++);
    LUNATIC_CHECK(RemoteCreditsReturned(transport));
    DriveRemoteCreditReturns(mock, false);
    TickLink(mock, transport, link, time++);
    LUNATIC_CHECK(!mock.chi_rn0_txlinkactiveack);
    LUNATIC_CHECK(!mock.chi_rn0_rxsactive);

    ActivateLink(mock, transport, link, time);
    LUNATIC_CHECK(mock.chi_rn0_rxsactive);
    LUNATIC_CHECK(transport.ValidateCreditAccounting());
    const auto status = transport.GetPortStatus(0);
    for (size_t index = 0; index < 3; ++index)
    {
        LUNATIC_CHECK(status[index].txCredits == 4);
        LUNATIC_CHECK(status[index].txQueue == 0);
        LUNATIC_CHECK(status[index + 3].rxCredits == 4);
        LUNATIC_CHECK(status[index + 3].rxQueue == 0);
    }
}

LUNATIC_TEST(Stage9LunaticRemoteDeactivationReturnsFinalProtocol)
{
    LinkMock mock;
    LinkTransport transport(&mock);
    LinkControl link(&mock, &transport);
    uint64_t time = 0;
    ActivateLink(mock, transport, link, time);

    Cohestra::CHI::RSPFlit response{};
    response.SrcID() = 0;
    response.TgtID() = 1;
    response.TxnID() = 7;
    response.Opcode() = ::CHI::Eb::Opcodes::RSP::Comp;
    LUNATIC_CHECK(transport.PushRXRSP(0, response));
    mock.chi_rn0_rxlinkactiveack = 0;
    TickLink(mock, transport, link, time++);
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_flitv);
    LUNATIC_CHECK(mock.chi_rn0_rxrsp_bits_Opcode == ::CHI::Eb::Opcodes::RSP::Comp);

    for (size_t cycle = 0; cycle < 8 && !LocalCreditsReturned(transport); ++cycle)
        TickLink(mock, transport, link, time++);
    LUNATIC_CHECK(LocalCreditsReturned(transport));
    LUNATIC_CHECK(!mock.chi_rn0_rxlinkactivereq);

    mock.chi_rn0_txlinkactivereq = 0;
    DriveRemoteCreditReturns(mock, true);
    for (size_t cycle = 0; cycle < 8 && !RemoteCreditsReturned(transport); ++cycle)
        TickLink(mock, transport, link, time++);
    LUNATIC_CHECK(RemoteCreditsReturned(transport));
    DriveRemoteCreditReturns(mock, false);
    TickLink(mock, transport, link, time++);
    LUNATIC_CHECK(!mock.chi_rn0_txlinkactiveack);
    LUNATIC_CHECK(!mock.chi_rn0_rxsactive);
    LUNATIC_CHECK(transport.ValidateCreditAccounting());
}
