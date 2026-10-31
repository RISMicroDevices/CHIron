#include "cohestra_lunatic_mock.hpp"
#include "common.hpp"

using namespace Cohestra;
using namespace LunaticTest;

namespace {

    LunaticModel::Configuration MakeConfiguration(::CHI::Lunatic::Options options = {})
    {
        LunaticModel::Configuration configuration;
        configuration.homeNodeId = 0;
        configuration.options = options;
        configuration.rnPorts = { { 0, 1 }, { 1, 2 } };
        return configuration;
    }

    Cohestra::CHI::REQFlit MakeRead(Cohestra::LunaticModel::NodeId source,
        Cohestra::LunaticModel::NodeId target, uint16_t txnId, uint64_t address)
    {
        Cohestra::CHI::REQFlit request{};
        request.SrcID() = source;
        request.TgtID() = target;
        request.TxnID() = txnId;
        request.Opcode() = ::CHI::Eb::Opcodes::REQ::ReadNoSnp;
        request.Size() = 6;
        request.Addr() = address;
        request.AllowRetry() = 1;
        return request;
    }

    template<typename Predicate>
    uint64_t RunUntil(LunaticModel& model, Predicate predicate, uint64_t start = 2, size_t limit = 256)
    {
        for (uint64_t time = start; time < start + limit * 2; time += 2)
        {
            model.Tick(time);
            if (predicate())
                return time;
        }
        LUNATIC_CHECK(false);
        return start;
    }

    void DrainOutputs(FakeCHIToRNInterface& transport, size_t port = 0)
    {
        while (transport.TakeRSP(port)) { }
        while (transport.TakeDAT(port)) { }
        while (transport.TakeSNP(port)) { }
    }

}

LUNATIC_TEST(Stage9CohestraConfigurationValidation)
{
    FakeCHIToRNInterface transport;
    auto configuration = MakeConfiguration();
    configuration.rnPorts.clear();
    LunaticTest::CheckThrows<std::invalid_argument>([&] { LunaticModel model(transport, configuration); });

    configuration = MakeConfiguration();
    configuration.rnPorts[1].nodeId = 1;
    LunaticTest::CheckThrows<std::invalid_argument>([&] { LunaticModel model(transport, configuration); });

    configuration = MakeConfiguration();
    configuration.rnPorts[1].port = 0;
    LunaticTest::CheckThrows<std::invalid_argument>([&] { LunaticModel model(transport, configuration); });

    configuration = MakeConfiguration();
    configuration.rnPorts[0].nodeId = 0;
    LunaticTest::CheckThrows<std::invalid_argument>([&] { LunaticModel model(transport, configuration); });

    configuration = MakeConfiguration();
    configuration.rnPorts[0].port = 7;
    LunaticTest::CheckThrows<std::invalid_argument>([&] { LunaticModel model(transport, configuration); });

    configuration = MakeConfiguration();
    configuration.options.mteEnabled = true;
    LunaticTest::CheckThrows<std::invalid_argument>([&] { LunaticModel model(transport, configuration); });

    configuration = MakeConfiguration();
    configuration.options.dvmEnabled = true;
    LunaticTest::CheckThrows<std::invalid_argument>([&] { LunaticModel model(transport, configuration); });

    configuration = MakeConfiguration();
    configuration.options.backend = ::CHI::Lunatic::BackendKind::CHI;
    LunaticTest::CheckThrows<std::invalid_argument>([&] { LunaticModel model(transport, configuration); });
}

LUNATIC_TEST(Stage9CohestraReadAndTimeScaling)
{
    FakeCHIToRNInterface transport;
    auto configuration = MakeConfiguration();
    configuration.options.tagLatency = 1;
    configuration.options.dataLatency = 1;
    configuration.options.memoryLatency = 1;
    LunaticModel model(transport, configuration);

    transport.InjectREQ(0, MakeRead(1, 0, 7, 0x80000040));
    model.Tick(2);
    LUNATIC_CHECK(!transport.HasTXREQ(0));
    LUNATIC_CHECK(model.GetNode().GetTime() == 1);
    model.Tick(2);
    LUNATIC_CHECK(model.GetNode().GetTime() == 1);

    size_t beats = 0;
    RunUntil(model, [&] {
        while (const auto data = transport.TakeDAT(0))
        {
            LUNATIC_CHECK(data->TgtID() == 1);
            LUNATIC_CHECK(data->TxnID() == 7);
            LUNATIC_CHECK(data->DataID() == beats * 2);
            ++beats;
        }
        return beats == 2;
    }, 4);
    DrainOutputs(transport);
    LUNATIC_CHECK(model.Drain());
    LUNATIC_CHECK(model.Validate());

    model.Tick(402);
    model.Tick(400);
    LUNATIC_CHECK(model.HasErrors());
    model.Reset();
    LUNATIC_CHECK(!model.HasErrors());
    LUNATIC_CHECK(model.GetNode().GetTime() == 0);
}

LUNATIC_TEST(Stage9CohestraInputBackpressureIsLossless)
{
    FakeCHIToRNInterface transport;
    auto configuration = MakeConfiguration();
    configuration.options.inputQueueLimit = 1;
    configuration.options.tagLatency = 1;
    configuration.options.dataLatency = 1;
    configuration.options.memoryLatency = 1;
    LunaticModel model(transport, configuration);

    transport.InjectREQ(0, MakeRead(1, 0, 1, 0x80000000));
    transport.InjectREQ(0, MakeRead(1, 0, 2, 0x80000080));
    model.Tick(2);
    LUNATIC_CHECK(transport.HasTXREQ(0));
    const uint64_t accepted = RunUntil(model, [&] { return !transport.HasTXREQ(0); }, 4);

    size_t beats = 0;
    RunUntil(model, [&] {
        while (const auto data = transport.TakeDAT(0))
        {
            LUNATIC_CHECK(data->TxnID() == 1 || data->TxnID() == 2);
            ++beats;
        }
        return beats == 4;
    }, accepted + 2);
    DrainOutputs(transport);
    LUNATIC_CHECK(model.Drain());
    LUNATIC_CHECK(model.Validate());
}

LUNATIC_TEST(Stage9CohestraOutputBackpressureIsLossless)
{
    FakeCHIToRNInterface transport;
    transport.outputLimit = 1;
    auto configuration = MakeConfiguration();
    configuration.options.tagLatency = 1;
    configuration.options.dataLatency = 1;
    configuration.options.memoryLatency = 1;
    LunaticModel model(transport, configuration);

    transport.InjectREQ(0, MakeRead(1, 0, 9, 0x80000100));
    const uint64_t ready = RunUntil(model, [&] { return transport.ports[0].rxdat.size() == 1; });
    model.Tick(ready + 2);
    LUNATIC_CHECK(transport.ports[0].rxdat.size() == 1);

    const auto first = transport.TakeDAT(0);
    LUNATIC_CHECK(first && first->TxnID() == 9);
    model.Tick(ready + 4);
    const auto second = transport.TakeDAT(0);
    LUNATIC_CHECK(second && second->TxnID() == 9);
    LUNATIC_CHECK(second->DataID() != first->DataID());
    DrainOutputs(transport);
    LUNATIC_CHECK(model.Drain());
    LUNATIC_CHECK(model.Validate());
}

LUNATIC_TEST(Stage9CohestraPeerDataAndUnknownTarget)
{
    FakeCHIToRNInterface transport;
    LunaticModel model(transport, MakeConfiguration());

    Cohestra::CHI::DATFlit data{};
    data.SrcID() = 1;
    data.TgtID() = 2;
    data.TxnID() = 3;
    data.Opcode() = ::CHI::Eb::Opcodes::DAT::SnpRespDataFwded;
    transport.InjectDAT(0, data);
    model.Tick(2);
    const auto forwarded = transport.TakeDAT(1);
    LUNATIC_CHECK(forwarded && forwarded->TgtID() == 2);
    LUNATIC_CHECK(!transport.HasTXDAT(0));
    LUNATIC_CHECK(!model.HasErrors());

    data.TgtID() = 7;
    transport.InjectDAT(0, data);
    model.Tick(4);
    LUNATIC_CHECK(transport.HasTXDAT(0));
    LUNATIC_CHECK(model.HasErrors());
    LUNATIC_CHECK(!model.GetIntegrationErrors().empty());
    LUNATIC_CHECK(!model.Validate());
}

LUNATIC_TEST(Stage9CohestraRetryAndCreditGrant)
{
    FakeCHIToRNInterface transport;
    auto configuration = MakeConfiguration();
    configuration.events = std::make_shared<::CHI::Lunatic::EventHub>();
    LunaticModel model(transport, configuration);

    bool retry = true;
    model.GetNode().GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<::CHI::Lunatic::RequestAdmissionEvent>("force-retry", 0,
            [&](::CHI::Lunatic::RequestAdmissionEvent& event) {
                if (retry && event.request.allowRetry)
                {
                    retry = false;
                    event.RequestRetry();
                }
            }));

    transport.InjectREQ(0, MakeRead(1, 0, 4, 0x80000200));
    const uint64_t granted = RunUntil(model, [&] { return transport.ports[0].rxrsp.size() == 2; });
    LUNATIC_CHECK(!retry);
    const auto retryAck = transport.TakeRSP(0);
    const auto creditGrant = transport.TakeRSP(0);
    LUNATIC_CHECK(retryAck && retryAck->Opcode() == ::CHI::Eb::Opcodes::RSP::RetryAck);
    LUNATIC_CHECK(creditGrant && creditGrant->Opcode() == ::CHI::Eb::Opcodes::RSP::PCrdGrant);

    Cohestra::CHI::REQFlit creditReturn{};
    creditReturn.SrcID() = 1;
    creditReturn.TgtID() = 0;
    creditReturn.Opcode() = ::CHI::Eb::Opcodes::REQ::PCrdReturn;
    creditReturn.PCrdType() = creditGrant->PCrdType();
    transport.InjectREQ(0, creditReturn);
    RunUntil(model, [&] { return model.GetNode().GetPerformanceCounters().returnedCredits == 1; }, granted + 2);
    DrainOutputs(transport);
    LUNATIC_CHECK(model.Drain());
    LUNATIC_CHECK(model.Validate());
}
