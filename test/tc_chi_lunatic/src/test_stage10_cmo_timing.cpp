#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    Options CmoOptions()
    {
        Options options = MakeType3Options();
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.poisonEnabled = false;
        options.dataCheckEnabled = false;
        return options;
    }

    Transaction& PrepareCmo(Core::Impl& impl, uint16_t opcode, bool homeDirty)
    {
        Request request;
        request.source = 4;
        request.target = 2;
        request.txnId = 1;
        request.opcode = opcode;
        request.address = 0x1000;
        request.size = 6;
        request.allowRetry = false;
        request.snpAttr = true;
        request.memAttr = CHI::Eb::MemAttrs::WriteBack_Allocate;
        impl.AdmitRequest(request);
        Transaction& transaction = *impl.tracker.FindRequest(4, 1);
        const LineKey line = transaction.request.Line();
        const bool homeValid = impl.options.policy != PolicyKind::Exclusive;
        if (homeValid)
            impl.InstallHome(line, 0, LineData{}, homeDirty, impl.time);
        transaction.homeHit = homeValid;
        FlowState& flow = impl.flows.at(transaction.handle);
        flow.homeDirty = homeValid && homeDirty;
        flow.validBytes.fill(1);
        flow.snoopLine = line;
        impl.Bank(line).snoopFilter.Track(line, 4, CacheState::UC, impl.time);
        impl.ResolveCoherent(transaction);
        return transaction;
    }

    void CheckLocalCmo(const Options& options, uint16_t opcode, bool homeDirty)
    {
        Core::Impl impl(2, options, std::make_shared<EventHub>());
        Transaction& transaction = PrepareCmo(impl, opcode, homeDirty);
        impl.ActionCoherent(transaction);
        LUNATIC_CHECK(impl.txreq.empty());
        LUNATIC_CHECK(impl.txrsp.size() == 1);
        const auto response = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
        LUNATIC_CHECK(response && response->opcode == CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(response->resp == CHI::Eb::Resps::Comp_I);
        impl.Retire(transaction.handle);
        LUNATIC_CHECK(impl.errors.empty());
        LUNATIC_CHECK(impl.ValidateInvariants());
        LUNATIC_CHECK(!impl.HasInflight());
    }

    void CheckDirtyCmo(uint16_t opcode)
    {
        Core::Impl impl(2, CmoOptions(), std::make_shared<EventHub>());
        Transaction& transaction = PrepareCmo(impl, opcode, true);
        LUNATIC_CHECK(transaction.readyTime == 2);
        impl.time = transaction.readyTime;
        impl.ActionCoherent(transaction);
        LUNATIC_CHECK(impl.txreq.size() == 1);
        LUNATIC_CHECK(impl.txreq.front().readyTime == 2);
        LUNATIC_CHECK(impl.txreq.front().flit.opcode == CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
        LUNATIC_CHECK(impl.txrsp.empty());
        LUNATIC_CHECK(impl.flows.at(transaction.handle).backendOperationOwned);
        const auto request = impl.Pop(impl.txreq, EventKind::EmittedTXREQ);
        LUNATIC_CHECK(request.has_value());

        Response completion;
        completion.source = 6;
        completion.target = 2;
        completion.txnId = transaction.handle;
        completion.opcode = CHI::Eb::Opcodes::RSP::CompDBIDResp;
        completion.dbid = 0xa5;
        LUNATIC_CHECK(impl.HandleBackendRSP(transaction, completion));
        for (size_t beat = 0; beat < BeatsPerLine(impl.options); ++beat)
        {
            impl.ActionCoherent(transaction);
            LUNATIC_CHECK(!impl.txdat.empty());
            impl.time = impl.txdat.front().readyTime;
            const auto data = impl.Pop(impl.txdat, EventKind::EmittedTXDAT, true);
            LUNATIC_CHECK(data && data->opcode == CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        }
        LUNATIC_CHECK(!impl.flows.at(transaction.handle).backendOperationOwned);
        impl.ActionCoherent(transaction);
        LUNATIC_CHECK(impl.txreq.empty());
        LUNATIC_CHECK(impl.txrsp.size() == 1);
        const auto response = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
        LUNATIC_CHECK(response && response->opcode == CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(response->resp == CHI::Eb::Resps::Comp_I);
        impl.Retire(transaction.handle);
        LUNATIC_CHECK(impl.errors.empty());
        LUNATIC_CHECK(impl.ValidateInvariants());
        LUNATIC_CHECK(!impl.HasInflight());
    }

}

LUNATIC_TEST(Stage10CmoBackendWhenRequired)
{
    CheckLocalCmo(CmoOptions(), CHI::Eb::Opcodes::REQ::CleanShared, false);
    CheckLocalCmo(CmoOptions(), CHI::Eb::Opcodes::REQ::CleanInvalid, false);
    CheckLocalCmo(CmoOptions(), CHI::Eb::Opcodes::REQ::MakeInvalid, true);
    CheckDirtyCmo(CHI::Eb::Opcodes::REQ::CleanShared);
    CheckDirtyCmo(CHI::Eb::Opcodes::REQ::CleanInvalid);
    CheckLocalCmo(CmoOptions(), CHI::Eb::Opcodes::REQ::Evict, false);
}

LUNATIC_TEST(Stage10CmoLegacyStillForwardsCleanOperation)
{
    Options legacy;
    legacy.backend = BackendKind::CHI;
    legacy.subordinateId = 6;
    Core::Impl impl(2, legacy, std::make_shared<EventHub>());
    Transaction& transaction = PrepareCmo(impl, CHI::Eb::Opcodes::REQ::CleanShared, false);
    impl.ActionCoherent(transaction);
    LUNATIC_CHECK(impl.txreq.size() == 1);
    LUNATIC_CHECK(impl.txreq.front().flit.opcode == CHI::Eb::Opcodes::REQ::CleanShared);
    LUNATIC_CHECK(impl.txrsp.empty());
    const auto request = impl.Pop(impl.txreq, EventKind::EmittedTXREQ);
    LUNATIC_CHECK(request.has_value());
    impl.Retire(transaction.handle);
    LUNATIC_CHECK(impl.errors.empty());
    LUNATIC_CHECK(impl.ValidateInvariants());
    LUNATIC_CHECK(!impl.HasInflight());
}
