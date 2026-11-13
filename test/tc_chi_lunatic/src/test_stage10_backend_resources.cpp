#include "driver.hpp"

#include <array>
#include <map>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    Options BackendOptions(BackendKind backend, size_t banks = 1)
    {
        Options options = MakeType3Options();
        options.backend = backend;
        options.subordinateId = 6;
        options.poisonEnabled = false;
        options.dataCheckEnabled = false;
        options.policy = PolicyKind::NINE;
        options.inputQueueLimit = 96;
        options.outputQueueLimit = 96;
        options.structure.bankCount = banks;
        options.structure.requestBufferLimit = 0;
        options.structure.responseResourceLimit = 0;
        options.structure.memoryResourceLimit = 0;
        options.structure.refillResourceLimit = 0;
        return options;
    }

    Request MakeRequest(uint16_t opcode, NodeId source, TransactionId txnId, const LineKey& line)
    {
        Request request;
        request.source = source;
        request.target = 2;
        request.txnId = txnId;
        request.opcode = opcode;
        request.address = line.address;
        request.size = 6;
        request.allowRetry = false;
        const auto requestClass = ClassifyRequest(opcode);
        request.expCompAck = requestClass == RequestClass::CoherentRead ||
            requestClass == RequestClass::MakeUnique || requestClass == RequestClass::MakeReadUnique ||
            requestClass == RequestClass::CleanUnique;
        request.snpAttr = IsCoherentRequest(opcode) || opcode == CHI::Eb::Opcodes::REQ::ReadOnce;
        request.memAttr = requestClass == RequestClass::Atomic
            ? CHI::Eb::MemAttrs::NonCacheable_Bufferable : CHI::Eb::MemAttrs::WriteBack_Allocate;
        return request;
    }

    LineKey ResourceLine(size_t bank, size_t alias = 0)
    {
        return LineKey::FromAddress(0xa0000 + bank * LINE_SIZE + alias * 4 * LINE_SIZE, false);
    }

    class Harness {
    public:
        std::shared_ptr<EventHub> events = std::make_shared<EventHub>();
        Core::Impl impl;

        explicit Harness(const Options& options)
            : impl(2, options, events)
        {
        }

        Transaction& Add(uint16_t opcode, NodeId source, TransactionId txnId, const LineKey& line)
        {
            const Request request = MakeRequest(opcode, source, txnId, line);
            impl.AdmitRequest(request);
            Transaction* transaction = impl.tracker.FindRequest(source, txnId);
            LUNATIC_CHECK(transaction);
            transaction->dbid = transaction->handle;
            LUNATIC_CHECK(impl.tracker.BindDBID(transaction->handle, transaction->handle));
            FlowState& flow = impl.flows.at(transaction->handle);
            flow.expectedBeatMask = (uint64_t(1) << BeatsPerLine(impl.options)) - 1;
            return *transaction;
        }

        std::optional<Response> PopRSP()
        {
            return impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
        }

        std::optional<Data> PopDAT()
        {
            if (impl.txdat.empty())
                return std::nullopt;
            const Time ready = impl.txdat.front().readyTime;
            while (impl.time < ready)
            {
                ++impl.time;
                impl.Tick();
            }
            return impl.Pop(impl.txdat, EventKind::EmittedTXDAT);
        }

        std::optional<Request> PopREQ()
        {
            return impl.Pop(impl.txreq, EventKind::EmittedTXREQ);
        }

        void Check()
        {
            LUNATIC_CHECK(impl.ValidateInvariants());
        }
    };

    Response BackendResponse(const Transaction& transaction, uint16_t opcode)
    {
        Response response;
        response.source = 6;
        response.target = 2;
        response.txnId = transaction.handle;
        response.opcode = opcode;
        response.dbid = 1;
        return response;
    }

    Data BackendData(const Transaction& transaction, size_t beat, uint64_t address)
    {
        Data data;
        data.source = 6;
        data.target = 2;
        data.homeNid = 2;
        data.txnId = transaction.handle;
        data.opcode = CHI::Eb::Opcodes::DAT::CompData;
        data.dataId = beat * 2;
        data.ccid = (address >> 4) & 3;
        data.byteEnables.fill(1);
        return data;
    }

    void CompleteWriteJoin(Harness& harness, Transaction& transaction, size_t compAfterBeat,
                           bool popRequest = true)
    {
        if (popRequest)
            LUNATIC_CHECK(harness.PopREQ());
        LUNATIC_CHECK(harness.impl.HandleBackendRSP(transaction,
            BackendResponse(transaction, CHI::Eb::Opcodes::RSP::DBIDResp)));
        if (compAfterBeat == 0)
            LUNATIC_CHECK(harness.impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
        for (size_t beat = 0; beat < 2; ++beat)
        {
            harness.impl.AdvanceBackendWrite(transaction);
            LUNATIC_CHECK(harness.PopDAT());
            if (compAfterBeat == 1 && beat == 0)
                LUNATIC_CHECK(harness.impl.HandleBackendRSP(transaction,
                    BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
            if (beat == 0 && compAfterBeat != 0)
                LUNATIC_CHECK(harness.impl.flows.at(transaction.handle).backendOperationOwned);
        }
        if (compAfterBeat == 2)
            LUNATIC_CHECK(harness.impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
        LUNATIC_CHECK(!harness.impl.flows.at(transaction.handle).backendOperationOwned);
    }

}

LUNATIC_TEST(Stage10BackendResourcesStructuralOriginalCapacity)
{
    Options options = BackendOptions(BackendKind::Internal, 2);
    options.profile = ProfileKind::Custom;
    options.transactionLimit = 4;
    Driver<Config256> driver(options);
    for (size_t index = 0; index < 2; ++index)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp,
            4 + index, 10 + index, ResourceLine(index % 2).address, false);
        request.AllowRetry() = 0;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        driver.node.Tick(0);
    }
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 2);
    auto blocked = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 8, 20, ResourceLine(0, 1).address, false);
    blocked.AllowRetry() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(blocked));
    driver.node.Tick(0);
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 3);
    auto other = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 9, 21, ResourceLine(1, 1).address, false);
    other.AllowRetry() = 0;
    LUNATIC_CHECK(driver.node.PushRXREQ(other));
    driver.node.Tick(0);
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 4);
    LUNATIC_CHECK(driver.node.ValidateInvariants());
    driver.node.Reset();
}

LUNATIC_TEST(Stage10BackendResourcesSharedResponseHoldAndRelease)
{
    Options options = BackendOptions(BackendKind::Internal, 2);
    options.structure.responseResourceLimit = 1;
    Harness harness(options);
    auto& first = harness.Add(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 100, ResourceLine(0, 0));
    auto& second = harness.Add(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 5, 101, ResourceLine(0, 1));
    auto& other = harness.Add(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 6, 102, ResourceLine(1, 0));

    harness.impl.QueueDBID(first);
    harness.impl.QueueDBID(second);
    harness.impl.QueueDBID(other);
    LUNATIC_CHECK(harness.impl.banks[0].responseUnits == 1);
    LUNATIC_CHECK(harness.impl.banks[1].responseUnits == 1);
    LUNATIC_CHECK(!harness.impl.flows.at(second.handle).dbidSent);
    LUNATIC_CHECK(harness.PopRSP());
    LUNATIC_CHECK(harness.PopRSP());
    LUNATIC_CHECK(harness.impl.flows.at(first.handle).responseOwned);

    first.receivedBeats = harness.impl.flows.at(first.handle).expectedBeatMask;
    first.Mark(COMPLETE_WRITE_DATA);
    harness.impl.flows.at(first.handle).backendPhase = BackendPhase::Done;
    harness.impl.QueueComp(first);
    LUNATIC_CHECK(harness.PopRSP());
    LUNATIC_CHECK(!harness.impl.flows.at(first.handle).responseOwned);
    harness.impl.QueueDBID(second);
    LUNATIC_CHECK(harness.impl.flows.at(second.handle).dbidSent);
    harness.Check();
    harness.impl.Retire(first.handle);
    harness.impl.Retire(second.handle);
    harness.impl.Retire(other.handle);
    harness.Check();
}

LUNATIC_TEST(Stage10BackendResourcesCopyBackEarlyResponseHeld)
{
    Options options = BackendOptions(BackendKind::Internal);
    options.structure.responseResourceLimit = 1;
    Harness harness(options);
    auto& copyback = harness.Add(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 110, ResourceLine(0, 0));
    auto& writer = harness.Add(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 5, 111, ResourceLine(0, 1));

    harness.impl.QueueComp(copyback, true);
    LUNATIC_CHECK(harness.PopRSP());
    LUNATIC_CHECK(harness.impl.flows.at(copyback.handle).responseOwned);
    harness.impl.QueueDBID(writer);
    LUNATIC_CHECK(!harness.impl.flows.at(writer.handle).dbidSent);

    FlowState& flow = harness.impl.flows.at(copyback.handle);
    copyback.byteEnables.fill(1);
    copyback.receivedBeats = flow.expectedBeatMask;
    copyback.Mark(COMPLETE_WRITE_DATA);
    flow.writeResp = CHI::Eb::Resps::CopyBackWrData_UD_PD;
    flow.dataDirty = true;
    LUNATIC_CHECK(harness.impl.CommitCopyback(copyback));
    harness.impl.Retire(copyback.handle);
    LUNATIC_CHECK(!harness.impl.flows.contains(copyback.handle));
    harness.impl.QueueDBID(writer);
    LUNATIC_CHECK(harness.impl.flows.at(writer.handle).dbidSent);
    harness.impl.Retire(writer.handle);
    harness.Check();
}

LUNATIC_TEST(Stage10BackendResourcesSeparatedResponseLegs)
{
    Options options = BackendOptions(BackendKind::Internal);
    options.structure.responseResourceLimit = 1;
    options.separatedReadResponse = true;
    Harness harness(options);
    auto& read = harness.Add(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 120, ResourceLine(0, 0));
    harness.impl.ReturnRead(read);
    LUNATIC_CHECK(harness.PopRSP());
    LUNATIC_CHECK(harness.impl.flows.at(read.handle).responseOwned);
    harness.impl.ReturnRead(read);
    LUNATIC_CHECK(harness.PopDAT());
    LUNATIC_CHECK(harness.impl.flows.at(read.handle).responseOwned);
    harness.impl.ReturnRead(read);
    LUNATIC_CHECK(harness.PopDAT());
    LUNATIC_CHECK(!harness.impl.flows.at(read.handle).responseOwned);
    harness.impl.Retire(read.handle);

    auto& stash = harness.Add(CHI::Eb::Opcodes::REQ::StashOnceSepUnique, 4, 121, ResourceLine(0, 1));
    harness.impl.QueueComp(stash);
    LUNATIC_CHECK(harness.PopRSP());
    LUNATIC_CHECK(harness.impl.flows.at(stash.handle).responseOwned);
    harness.impl.QueueStashDone(stash);
    LUNATIC_CHECK(harness.PopRSP());
    LUNATIC_CHECK(!harness.impl.flows.at(stash.handle).responseOwned);
    harness.impl.Retire(stash.handle);

    auto& persist = harness.Add(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 4, 122, ResourceLine(0, 2));
    harness.impl.QueueCompPersist(persist);
    LUNATIC_CHECK(harness.PopRSP());
    LUNATIC_CHECK(!harness.impl.flows.at(persist.handle).responseOwned);
    harness.impl.Retire(persist.handle);
    harness.Check();
}

LUNATIC_TEST(Stage10BackendResourcesAtomicDwtAndSpecialResponses)
{
    Options options = BackendOptions(BackendKind::Internal);
    options.structure.responseResourceLimit = 1;
    Harness harness(options);
    auto& atomic = harness.Add(CHI::Eb::Opcodes::REQ::AtomicSwap, 4, 130, ResourceLine(0, 0));
    const_cast<Request&>(atomic.request).size = 3;
    harness.impl.QueueDBID(atomic);
    LUNATIC_CHECK(harness.PopRSP());
    LUNATIC_CHECK(harness.impl.flows.at(atomic.handle).responseOwned);
    atomic.Mark(COMPLETE_WRITE_DATA);
    harness.impl.flows.at(atomic.handle).directoryCommitted = true;
    harness.impl.ReturnAtomic(atomic);
    LUNATIC_CHECK(harness.PopDAT());
    LUNATIC_CHECK(!harness.impl.flows.at(atomic.handle).responseOwned);
    harness.impl.Retire(atomic.handle);

    auto& dwt = harness.Add(CHI::Eb::Opcodes::REQ::WriteUniqueFull, 4, 131, ResourceLine(0, 1));
    FlowState& dwtFlow = harness.impl.flows.at(dwt.handle);
    dwtFlow.dwtActive = true;
    dwtFlow.directoryCommitted = true;
    dwtFlow.backendCompReceived = true;
    harness.impl.QueueComp(dwt);
    LUNATIC_CHECK(harness.PopRSP());
    LUNATIC_CHECK(!dwtFlow.responseOwned);
    harness.impl.Retire(dwt.handle);

    harness.impl.retries[0].ackPopped = true;
    harness.impl.QueueRetryCredit(0);
    LUNATIC_CHECK(harness.PopRSP());
    LUNATIC_CHECK(harness.impl.banks[0].responseUnits == 0);
    harness.Check();
}

LUNATIC_TEST(Stage10BackendResourcesInternalMemoryAndHomeHit)
{
    Options options = BackendOptions(BackendKind::Internal, 2);
    options.structure.memoryResourceLimit = 1;
    Harness harness(options);
    auto& first = harness.Add(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 140, ResourceLine(0, 0));
    auto& second = harness.Add(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 141, ResourceLine(0, 1));
    auto& other = harness.Add(CHI::Eb::Opcodes::REQ::ReadNoSnp, 6, 142, ResourceLine(1, 0));

    harness.impl.StartMemoryRead(first);
    harness.impl.StartMemoryRead(second);
    harness.impl.StartMemoryRead(other);
    LUNATIC_CHECK(harness.impl.banks[0].backendOperations == 1);
    LUNATIC_CHECK(harness.impl.banks[1].backendOperations == 1);
    LUNATIC_CHECK(!harness.impl.flows.at(second.handle).backendOperationOwned);
    harness.impl.CompleteMemoryRead(first);
    LUNATIC_CHECK(!harness.impl.flows.at(first.handle).backendOperationOwned);
    harness.impl.StartMemoryRead(second);
    LUNATIC_CHECK(harness.impl.flows.at(second.handle).backendOperationOwned);
    harness.impl.CompleteMemoryRead(second);
    harness.impl.CompleteMemoryRead(other);

    auto& hit = harness.Add(CHI::Eb::Opcodes::REQ::ReadShared, 7, 143, ResourceLine(0, 2));
    hit.homeHit = true;
    harness.impl.StartMemoryRead(hit);
    LUNATIC_CHECK(!harness.impl.flows.at(hit.handle).backendOperationOwned);
    LUNATIC_CHECK(hit.Complete(COMPLETE_MEMORY));

    auto& write = harness.Add(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 8, 144, ResourceLine(0, 3));
    harness.impl.StartBackendWrite(write);
    LUNATIC_CHECK(harness.impl.flows.at(write.handle).backendOperationOwned);
    harness.impl.CompleteMemoryRead(write);
    LUNATIC_CHECK(!harness.impl.flows.at(write.handle).backendOperationOwned);

    auto& canceled = harness.Add(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 9, 145, ResourceLine(0, 4));
    canceled.canceled = true;
    harness.impl.StartBackendWrite(canceled);
    LUNATIC_CHECK(!harness.impl.flows.at(canceled.handle).backendOperationOwned);
    harness.impl.Retire(first.handle);
    harness.impl.Retire(second.handle);
    harness.impl.Retire(other.handle);
    harness.impl.Retire(hit.handle);
    harness.impl.Retire(write.handle);
    harness.impl.Retire(canceled.handle);
    harness.Check();
}

LUNATIC_TEST(Stage10BackendResourcesChiRetryAndCompletionJoins)
{
    Options options = BackendOptions(BackendKind::CHI);
    options.structure.memoryResourceLimit = 2;
    Harness harness(options);
    auto& read = harness.Add(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 150, ResourceLine(0, 0));
    harness.impl.StartMemoryRead(read);
    LUNATIC_CHECK(harness.PopREQ());
    LUNATIC_CHECK(harness.impl.flows.at(read.handle).backendOperationOwned);
    LUNATIC_CHECK(harness.impl.HandleBackendRetry(read,
        BackendResponse(read, CHI::Eb::Opcodes::RSP::RetryAck)));
    harness.impl.backendCredits[{ 6, 0 }] = 1;
    harness.impl.AdvanceBackendRetries();
    LUNATIC_CHECK(harness.PopREQ());
    LUNATIC_CHECK(harness.impl.flows.at(read.handle).backendOperationOwned);
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(harness.impl.HandleBackendDAT(read, BackendData(read, beat, read.request.address)));
    LUNATIC_CHECK(!harness.impl.flows.at(read.handle).backendOperationOwned);
    harness.impl.Retire(read.handle);

    for (size_t mode = 0; mode < 3; ++mode)
    {
        auto& write = harness.Add(CHI::Eb::Opcodes::REQ::WriteNoSnpFull,
            10 + mode, 160 + mode, ResourceLine(0, mode + 1));
        harness.impl.StartBackendWrite(write);
        CompleteWriteJoin(harness, write, mode);
        harness.impl.Retire(write.handle);
    }
    harness.Check();
}

LUNATIC_TEST(Stage10BackendResourcesAxiRefusalAndCompletion)
{
    Options options = BackendOptions(BackendKind::AXI);
    options.structure.memoryResourceLimit = 2;
    options.structure.refillResourceLimit = 1;
    options.outputQueueLimit = 1;
    Harness harness(options);
    auto& first = harness.Add(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 170, ResourceLine(0, 0));
    auto& second = harness.Add(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 171, ResourceLine(0, 1));
    harness.impl.StartMemoryRead(first);
    FlowState& secondFlow = harness.impl.flows.at(second.handle);
    secondFlow.baselineFetch = true;
    secondFlow.baselineLine = second.request.Line();
    harness.impl.StartMemoryRead(second);
    LUNATIC_CHECK(!secondFlow.backendOperationOwned);
    LUNATIC_CHECK(!secondFlow.baselineOperationOwned);
    LUNATIC_CHECK(secondFlow.baselineFetch);

    const auto ar = harness.impl.axi.PopAR();
    LUNATIC_CHECK(ar);
    for (size_t beat = 0; beat < 2; ++beat)
    {
        AXIReadData data;
        data.id = ar->id;
        data.last = beat == 1;
        LUNATIC_CHECK(harness.impl.axi.PushR(data));
        harness.impl.axi.Tick();
        while (const auto completion = harness.impl.axi.PeekCompletion())
        {
            harness.impl.HandleAXICompletion(*completion);
            harness.impl.axi.PopCompletion();
        }
    }
    LUNATIC_CHECK(!harness.impl.flows.at(first.handle).backendOperationOwned);
    harness.impl.Retire(first.handle);
    harness.impl.Retire(second.handle);

    auto& write = harness.Add(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 6, 172, ResourceLine(0, 2));
    harness.impl.StartBackendWrite(write);
    const auto aw = harness.impl.axi.PopAW();
    LUNATIC_CHECK(aw);
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(harness.impl.axi.PopW());
    AXIWriteResponse response;
    response.id = aw->id;
    LUNATIC_CHECK(harness.impl.axi.PushB(response));
    harness.impl.axi.Tick();
    while (const auto completion = harness.impl.axi.PeekCompletion())
    {
        harness.impl.HandleAXICompletion(*completion);
        harness.impl.axi.PopCompletion();
    }
    LUNATIC_CHECK(!harness.impl.flows.at(write.handle).backendOperationOwned);
    harness.impl.Retire(write.handle);
    harness.Check();
}

LUNATIC_TEST(Stage10BackendResourcesBaselineInstallAndRepeatedFetch)
{
    Options options = BackendOptions(BackendKind::CHI);
    options.structure.refillResourceLimit = 1;
    Harness harness(options);
    auto& copyback = harness.Add(CHI::Eb::Opcodes::REQ::WriteBackPtl, 4, 180, ResourceLine(0, 0));
    copyback.byteEnables.fill(1);
    FlowState& flow = harness.impl.flows.at(copyback.handle);
    flow.baselineFetch = true;
    flow.baselineLine = copyback.request.Line();
    flow.writeResp = CHI::Eb::Resps::CopyBackWrData_UD_PD;
    flow.dataDirty = true;
    harness.impl.StartMemoryRead(copyback);
    LUNATIC_CHECK(flow.backendOperationOwned);
    LUNATIC_CHECK(flow.baselineOperationOwned);
    LUNATIC_CHECK(!harness.impl.FetchBaseline(copyback));
    LUNATIC_CHECK(harness.impl.banks[0].refillOperations == 1);
    LUNATIC_CHECK(harness.PopREQ());
    for (size_t beat = 0; beat < 2; ++beat)
        LUNATIC_CHECK(harness.impl.HandleBackendDAT(copyback,
            BackendData(copyback, beat, flow.baselineLine.address)));
    LUNATIC_CHECK(!flow.backendOperationOwned);
    LUNATIC_CHECK(flow.baselineOperationOwned);
    LUNATIC_CHECK(harness.impl.FetchBaseline(copyback));
    LUNATIC_CHECK(harness.impl.banks[0].refillOperations == 1);
    LUNATIC_CHECK(harness.impl.CommitCopyback(copyback));
    LUNATIC_CHECK(!flow.baselineOperationOwned);
    harness.impl.Retire(copyback.handle);
    harness.Check();
}

LUNATIC_TEST(Stage10BackendResourcesSpillRetryAndMirror)
{
    Options options = BackendOptions(BackendKind::CHI);
    options.structure.memoryResourceLimit = 1;
    Harness harness(options);
    auto& write = harness.Add(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 190, ResourceLine(0, 0));
    FlowState& flow = harness.impl.flows.at(write.handle);
    flow.spillPending = true;
    flow.spillLine = write.request.Line();
    flow.spillData.fill(0x5a);
    flow.spillEnables.fill(1);
    harness.impl.StartBackendWrite(write);
    LUNATIC_CHECK(harness.PopREQ());
    LUNATIC_CHECK(harness.impl.HandleBackendRetry(write,
        BackendResponse(write, CHI::Eb::Opcodes::RSP::RetryAck)));
    LUNATIC_CHECK(flow.backendOperationOwned);
    harness.impl.backendCredits[{ 6, 0 }] = 1;
    harness.impl.AdvanceBackendRetries();
    LUNATIC_CHECK(harness.PopREQ());
    LUNATIC_CHECK(flow.backendOperationOwned);
    CompleteWriteJoin(harness, write, 2, false);
    LineImage image = MakeLineImage(write, harness.impl.options);
    LUNATIC_CHECK(harness.impl.WriteLineImage(write, flow.spillLine, image, flow.spillEnables));
    LUNATIC_CHECK(!flow.spillPending);
    LUNATIC_CHECK(!flow.backendOperationOwned);
    harness.impl.Retire(write.handle);
    harness.Check();
}

LUNATIC_TEST(Stage10BackendResourcesDirectCmoPersistAtomic)
{
    Options options = BackendOptions(BackendKind::CHI);
    options.structure.memoryResourceLimit = 1;
    options.strategy.cmoBackendWhenRequired = false;
    Harness harness(options);
    auto& cmo = harness.Add(CHI::Eb::Opcodes::REQ::CleanInvalid, 4, 200, ResourceLine(0, 0));
    harness.impl.ActionCoherent(cmo);
    LUNATIC_CHECK(harness.impl.flows.at(cmo.handle).backendOperationOwned);
    LUNATIC_CHECK(harness.PopREQ());
    LUNATIC_CHECK(harness.impl.HandleBackendRSP(cmo,
        BackendResponse(cmo, CHI::Eb::Opcodes::RSP::Comp)));
    LUNATIC_CHECK(!harness.impl.flows.at(cmo.handle).backendOperationOwned);
    harness.impl.Retire(cmo.handle);

    auto& persist = harness.Add(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 4, 201, ResourceLine(0, 1));
    harness.impl.ActionPersistence(persist);
    LUNATIC_CHECK(harness.impl.flows.at(persist.handle).backendOperationOwned);
    LUNATIC_CHECK(harness.PopREQ());
    LUNATIC_CHECK(harness.impl.HandleBackendRSP(persist,
        BackendResponse(persist, CHI::Eb::Opcodes::RSP::CompPersist)));
    LUNATIC_CHECK(!harness.impl.flows.at(persist.handle).backendOperationOwned);
    harness.impl.Retire(persist.handle);

    auto& atomic = harness.Add(CHI::Eb::Opcodes::REQ::AtomicSwap, 4, 202, ResourceLine(0, 2));
    harness.impl.StartAtomicBackend(atomic);
    LUNATIC_CHECK(harness.impl.flows.at(atomic.handle).backendOperationOwned);
    harness.impl.Retire(atomic.handle);
    LUNATIC_CHECK(harness.impl.banks[0].backendOperations == 0);
    harness.Check();
}

LUNATIC_TEST(Stage10BackendResourcesDmtCompletionAndReset)
{
    Options options = BackendOptions(BackendKind::CHI);
    options.dmtEnabled = true;
    options.structure.responseResourceLimit = 1;
    Harness harness(options);
    auto& read = harness.Add(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 210, ResourceLine(0, 0));
    const_cast<Request&>(read.request).order = 1;
    FlowState& flow = harness.impl.flows.at(read.handle);
    flow.dmtActive = true;
    harness.impl.StartMemoryRead(read);
    LUNATIC_CHECK(harness.PopREQ());
    LUNATIC_CHECK(harness.impl.HandleBackendRSP(read,
        BackendResponse(read, CHI::Eb::Opcodes::RSP::ReadReceipt)));
    LUNATIC_CHECK(!flow.backendOperationOwned);
    harness.impl.ReturnRead(read);
    LUNATIC_CHECK(harness.PopRSP());
    LUNATIC_CHECK(!flow.responseOwned);
    harness.impl.Retire(read.handle);

    auto& second = harness.Add(CHI::Eb::Opcodes::REQ::ReadNoSnp, 5, 211, ResourceLine(0, 1));
    harness.impl.StartMemoryRead(second);
    harness.impl.QueueDBID(second);
    LUNATIC_CHECK(harness.impl.banks[0].responseUnits == 1);
    LUNATIC_CHECK(harness.impl.banks[0].backendOperations == 1);
    harness.impl.Reset();
    LUNATIC_CHECK(!harness.impl.HasInflight());
    LUNATIC_CHECK(harness.impl.banks[0].responseUnits == 0);
    LUNATIC_CHECK(harness.impl.banks[0].backendOperations == 0);
    LUNATIC_CHECK(harness.impl.banks[0].refillOperations == 0);
    harness.Check();
}
