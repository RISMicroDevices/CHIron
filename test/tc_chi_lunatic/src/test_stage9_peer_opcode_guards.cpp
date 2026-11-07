#include "cohestra_lunatic_mock.hpp"
#include "common.hpp"

using namespace Cohestra;
using namespace LunaticTest;

namespace {

    LunaticModel::Configuration PeerConfiguration()
    {
        LunaticModel::Configuration configuration;
        configuration.homeNodeId = 0;
        configuration.rnPorts = { { 0, 1 }, { 1, 2 } };
        return configuration;
    }

}

LUNATIC_TEST(Stage9PeerCompDataForwardingControl)
{
    FakeCHIToRNInterface transport;
    LunaticModel model(transport, PeerConfiguration());
    Cohestra::CHI::DATFlit data{};
    data.SrcID() = 1;
    data.TgtID() = 2;
    data.TxnID() = 3;
    data.Opcode() = ::CHI::Eb::Opcodes::DAT::CompData;
    transport.InjectDAT(0, data);
    model.Tick(2);

    const auto forwarded = transport.TakeDAT(1);
    LUNATIC_CHECK(forwarded && forwarded->Opcode() == ::CHI::Eb::Opcodes::DAT::CompData);
    LUNATIC_CHECK(!transport.HasTXDAT(0));
    LUNATIC_CHECK(!model.HasErrors());
    LUNATIC_CHECK(model.Validate());
}

LUNATIC_TEST(Stage9PeerRejectsNonCompData)
{
    for (const uint16_t opcode : {
             ::CHI::Eb::Opcodes::DAT::SnpRespData,
             ::CHI::Eb::Opcodes::DAT::SnpRespDataPtl,
             ::CHI::Eb::Opcodes::DAT::SnpRespDataFwded,
             ::CHI::Eb::Opcodes::DAT::NonCopyBackWrData,
             ::CHI::Eb::Opcodes::DAT::NCBWrDataCompAck,
             ::CHI::Eb::Opcodes::DAT::CopyBackWrData,
             ::CHI::Eb::Opcodes::DAT::WriteDataCancel,
             ::CHI::Eb::Opcodes::DAT::DataSepResp
         })
    {
        FakeCHIToRNInterface transport;
        LunaticModel model(transport, PeerConfiguration());
        Cohestra::CHI::DATFlit data{};
        data.SrcID() = 1;
        data.TgtID() = 2;
        data.TxnID() = 3;
        data.Opcode() = opcode;
        transport.InjectDAT(0, data);
        model.Tick(2);

        LUNATIC_CHECK(!transport.TakeDAT(1));
        LUNATIC_CHECK(transport.HasTXDAT(0));
        LUNATIC_CHECK(model.HasErrors());
        LUNATIC_CHECK(!model.Validate());
    }
}

LUNATIC_TEST(Stage9PeerRejectsResponses)
{
    for (const uint16_t opcode : {
             ::CHI::Eb::Opcodes::RSP::SnpResp,
             ::CHI::Eb::Opcodes::RSP::SnpRespFwded,
             ::CHI::Eb::Opcodes::RSP::CompAck
         })
    {
        FakeCHIToRNInterface transport;
        LunaticModel model(transport, PeerConfiguration());
        Cohestra::CHI::RSPFlit response{};
        response.SrcID() = 1;
        response.TgtID() = 2;
        response.TxnID() = 3;
        response.Opcode() = opcode;
        transport.InjectRSP(0, response);
        model.Tick(2);

        LUNATIC_CHECK(!transport.TakeRSP(1));
        LUNATIC_CHECK(transport.HasTXRSP(0));
        LUNATIC_CHECK(model.HasErrors());
        LUNATIC_CHECK(!model.Validate());
    }
}
