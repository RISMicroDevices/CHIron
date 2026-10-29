#include "common.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

static_assert(CHI::Eb::Flits::REQ<Config256>::TXNID_RANGE.first == CHI::Eb::Flits::REQ<Config256>::TXNID_LSB);
static_assert(CHI::Eb::Flits::REQ<Config256>::TXNID_RANGE.second == CHI::Eb::Flits::REQ<Config256>::TXNID_MSB);
static_assert(CHI::Eb::Flits::RSP<Config256>::TXNID_RANGE.first == CHI::Eb::Flits::RSP<Config256>::TXNID_LSB);
static_assert(CHI::Eb::Flits::RSP<Config256>::TXNID_RANGE.second == CHI::Eb::Flits::RSP<Config256>::TXNID_MSB);
static_assert(CHI::Eb::Flits::DAT<Config256>::TXNID_RANGE.first == CHI::Eb::Flits::DAT<Config256>::TXNID_LSB);
static_assert(CHI::Eb::Flits::DAT<Config256>::TXNID_RANGE.second == CHI::Eb::Flits::DAT<Config256>::TXNID_MSB);

LUNATIC_TEST(Stage1Geometry)
{
    Options options;
    LUNATIC_CHECK(BytesPerBeat(options) == 32);
    LUNATIC_CHECK(BeatsPerLine(options) == 2);
    options.dataWidth = 128;
    LUNATIC_CHECK(BeatsPerLine(options) == 4);
    options.dataWidth = 512;
    LUNATIC_CHECK(BeatsPerLine(options) == 1);
}

LUNATIC_TEST(Stage1RejectsInvalidOptions)
{
    Options options;
    options.inputQueueLimit = 0;
    CheckThrows([&] { ValidateOptions(options); });
    options = {};
    options.transactionLimit = 4097;
    CheckThrows([&] { ValidateOptions(options); });
    options = {};
    options.dataWidth = 192;
    CheckThrows([&] { ValidateOptions(options); });
    options = {};
    options.tagLatency = 0;
    CheckThrows([&] { ValidateOptions(options); });
    options = {};
    options.dvmEnabled = true;
    CheckThrows([&] { ValidateOptions(options); });
    options = {};
    options.backend = BackendKind::AXI;
    CheckThrows([&] { ValidateOptions(options); });
}

LUNATIC_TEST(Stage1RejectsFlitGeometryMismatch)
{
    Options options;
    options.dataWidth = 128;
    CheckThrows([&] { HomeNode<Config256> node(2, options); });
}

LUNATIC_TEST(Stage1BoundedAcceptanceAndReset)
{
    auto events = std::make_shared<EventHub>();
    size_t accepted = 0;
    events->OnAcceptedRXREQ.Register(
        Gravity::MakeListener<AcceptedRXREQEvent>("accepted", 0, [&](AcceptedRXREQEvent& event) {
            LUNATIC_CHECK(event.kind == EventKind::AcceptedRXREQ);
            ++accepted;
        }));

    Options options;
    options.inputQueueLimit = 1;
    HomeNode<Config256> node(2, options, events);
    HomeNode<Config256>::REQFlit req{};
    req.SrcID() = 4;
    req.TgtID() = 2;
    req.TxnID() = 9;
    req.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    req.Addr() = 0x1040;
    req.Size() = 6;

    LUNATIC_CHECK(node.PushRXREQ(req));
    LUNATIC_CHECK(accepted == 1);
    LUNATIC_CHECK(!node.PushRXREQ(req));
    LUNATIC_CHECK(accepted == 1);
    LUNATIC_CHECK(node.HasInflight());
    node.Tick(3);
    LUNATIC_CHECK(accepted == 1);
    LUNATIC_CHECK(node.GetTime() == 3);
    LUNATIC_CHECK(!node.Drain());
    node.Reset();
    LUNATIC_CHECK(node.GetTime() == 0);
    LUNATIC_CHECK(!node.HasInflight());
    LUNATIC_CHECK(node.PushRXREQ(req));
    LUNATIC_CHECK(accepted == 2);
    node.Reset();
    LUNATIC_CHECK(node.Drain());
    LUNATIC_CHECK(accepted == 2);
}

LUNATIC_TEST(Stage1EventPriorityIsolationAndClear)
{
    auto events = std::make_shared<EventHub>();
    std::vector<int> order;
    size_t accepted = 0;
    events->OnReset.Register(
        Gravity::MakeListener<ResetEvent>("late", 10, [&](ResetEvent&) { order.push_back(10); }));
    events->OnReset.Register(
        Gravity::MakeListener<ResetEvent>("early", -10, [&](ResetEvent&) { order.push_back(-10); }));
    events->OnReset.Register(
        Gravity::MakeListener<ResetEvent>("middle", 0, [&](ResetEvent&) { order.push_back(0); }));
    events->OnAcceptedRXREQ.Register(
        Gravity::MakeListener<AcceptedRXREQEvent>("accepted", 0, [&](AcceptedRXREQEvent&) { ++accepted; }));

    ResetEvent event;
    events->OnReset.Fire(event);
    LUNATIC_CHECK((order == std::vector<int>{ -10, 0, 10 }));
    LUNATIC_CHECK(accepted == 0);
    LUNATIC_CHECK(!events->OnReset.empty());

    events->Clear();
    LUNATIC_CHECK(events->OnReset.empty());
    events->OnReset.Fire(event);
    LUNATIC_CHECK((order == std::vector<int>{ -10, 0, 10 }));
}

LUNATIC_TEST(Stage1PureEmptyPeek)
{
    HomeNode<Config256> node(2);
    auto events = node.GetEvents();
    size_t emitted = 0;
    events->OnEmittedTXREQ.Register(
        Gravity::MakeListener<EmittedTXREQEvent>("emitted", 0, [&](EmittedTXREQEvent&) { ++emitted; }));
    events->OnEmittedTXRSP.Register(
        Gravity::MakeListener<EmittedTXRSPEvent>("emitted", 0, [&](EmittedTXRSPEvent&) { ++emitted; }));
    events->OnEmittedTXDAT.Register(
        Gravity::MakeListener<EmittedTXDATEvent>("emitted", 0, [&](EmittedTXDATEvent&) { ++emitted; }));
    events->OnEmittedTXSNP.Register(
        Gravity::MakeListener<EmittedTXSNPEvent>("emitted", 0, [&](EmittedTXSNPEvent&) { ++emitted; }));

    LUNATIC_CHECK(!node.PeekTXREQ());
    LUNATIC_CHECK(!node.PeekTXRSP());
    LUNATIC_CHECK(!node.PeekTXDAT());
    LUNATIC_CHECK(!node.PeekTXSNP());
    LUNATIC_CHECK(emitted == 0);
    LUNATIC_CHECK(!node.PopTXREQ());
    LUNATIC_CHECK(!node.PopTXRSP());
    LUNATIC_CHECK(!node.PopTXDAT());
    LUNATIC_CHECK(!node.PopTXSNP());
    LUNATIC_CHECK(emitted == 0);
}

LUNATIC_TEST(Stage1FacadeForms)
{
    Options options128;
    options128.dataWidth = 128;
    HomeNode<Config128> narrow(2, options128);
    Options options512;
    options512.dataWidth = 512;
    Bridge<Config512> wide(2, options512);
    Interconnect<Config256> interconnect(2);

    size_t accepted = 0;
    interconnect.GetNode().GetEvents()->OnAcceptedRXREQ.Register(
        Gravity::MakeListener<AcceptedRXREQEvent>("accepted", 0, [&](AcceptedRXREQEvent&) { ++accepted; }));
    HomeNode<Config256>::REQFlit req{};
    req.SrcID() = 4;
    req.TgtID() = 2;
    req.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    req.Size() = 6;
    LUNATIC_CHECK(interconnect.PushRXREQ(req));
    LUNATIC_CHECK(accepted == 1);
    interconnect.Reset();
    LUNATIC_CHECK(interconnect.Drain());
    LUNATIC_CHECK(wide.Drain());
    narrow.Reset();
    LUNATIC_CHECK(narrow.Drain());
}
