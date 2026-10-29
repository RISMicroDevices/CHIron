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
    events->Register("accepted", 0, [&](const Event& event) {
        if (event.kind == EventKind::AcceptedRXREQ)
            ++accepted;
    });

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
    LUNATIC_CHECK(!node.PushRXREQ(req));
    LUNATIC_CHECK(node.HasInflight());
    LUNATIC_CHECK(accepted == 0);
    node.Tick(3);
    LUNATIC_CHECK(accepted == 1);
    LUNATIC_CHECK(node.GetTime() == 3);
    LUNATIC_CHECK(!node.Drain());
    node.Reset();
    LUNATIC_CHECK(node.GetTime() == 0);
    LUNATIC_CHECK(!node.HasInflight());
    LUNATIC_CHECK(node.Drain());
    LUNATIC_CHECK(accepted == 1);
}

LUNATIC_TEST(Stage1EventPriorityAndReentry)
{
    auto events = std::make_shared<EventHub>();
    std::vector<int> order;
    events->Register("late", 10, [&](const Event&) { order.push_back(10); });
    events->Register("early", -10, [&](const Event&) {
        order.push_back(-10);
        LUNATIC_CHECK(!events->Dispatch());
    });
    events->Register("middle", 0, [&](const Event&) { order.push_back(0); });

    Event event;
    event.kind = EventKind::Reset;
    events->Publish(event);
    LUNATIC_CHECK(events->Pending() == 1);
    LUNATIC_CHECK(events->Dispatch());
    LUNATIC_CHECK((order == std::vector<int>{ -10, 0, 10 }));
    LUNATIC_CHECK(events->ReentryRejected());
    LUNATIC_CHECK(events->Pending() == 0);
}

LUNATIC_TEST(Stage1PureEmptyPeek)
{
    HomeNode<Config256> node(2);
    auto events = node.GetEvents();
    LUNATIC_CHECK(!node.PeekTXREQ());
    LUNATIC_CHECK(!node.PeekTXRSP());
    LUNATIC_CHECK(!node.PeekTXDAT());
    LUNATIC_CHECK(!node.PeekTXSNP());
    LUNATIC_CHECK(events->Pending() == 0);
    LUNATIC_CHECK(!node.PopTXREQ());
    LUNATIC_CHECK(!node.PopTXRSP());
    LUNATIC_CHECK(!node.PopTXDAT());
    LUNATIC_CHECK(!node.PopTXSNP());
    LUNATIC_CHECK(events->Pending() == 0);
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

    HomeNode<Config256>::REQFlit req{};
    req.SrcID() = 4;
    req.TgtID() = 2;
    req.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    req.Size() = 6;
    LUNATIC_CHECK(interconnect.PushRXREQ(req));
    LUNATIC_CHECK(interconnect.GetNode().GetEvents()->Pending() == 1);
    interconnect.Reset();
    LUNATIC_CHECK(interconnect.Drain());
    LUNATIC_CHECK(wide.Drain());
    narrow.Reset();
    LUNATIC_CHECK(narrow.Drain());
}
