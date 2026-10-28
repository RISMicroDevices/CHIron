#include "common.hpp"

#include "chi/icn/lunatic/chi_lunatic_arbiter.hpp"
#include "chi/icn/lunatic/chi_lunatic_hazard.hpp"
#include "chi/icn/lunatic/chi_lunatic_memory.hpp"
#include "chi/icn/lunatic/chi_lunatic_tracker.hpp"
#include "chi/icn/lunatic/chi_lunatic_transaction.hpp"

#include <type_traits>

using namespace CHI::Lunatic;
using namespace LunaticTest;

static_assert(std::is_const_v<decltype(Transaction::handle)>);
static_assert(std::is_const_v<decltype(Transaction::request)>);
static_assert(std::is_const_v<decltype(Transaction::actionAge)>);

LUNATIC_TEST(Stage2TransactionSnapshotsAcceptedRequest)
{
    Request request;
    request.source = 4;
    request.target = 2;
    request.txnId = 19;
    request.address = 0x12345;
    request.opcode = 7;
    request.qos = 11;
    request.nonSecure = true;

    Transaction transaction(42, request, 9);
    request.source = 8;
    request.txnId = 20;
    request.address = 0;
    request.qos = 0;

    LUNATIC_CHECK(transaction.handle == 42);
    LUNATIC_CHECK(transaction.actionAge == 9);
    LUNATIC_CHECK(transaction.request.source == 4);
    LUNATIC_CHECK(transaction.request.target == 2);
    LUNATIC_CHECK(transaction.request.txnId == 19);
    LUNATIC_CHECK(transaction.request.address == 0x12345);
    LUNATIC_CHECK(transaction.request.qos == 11);
    LUNATIC_CHECK(transaction.request.Line() == LineKey::FromAddress(0x12345, true));
    LUNATIC_CHECK(transaction.phase == Phase::Receive);
    LUNATIC_CHECK(transaction.wait == WaitReason::None);
    LUNATIC_CHECK(transaction.data == LineData{});
    LUNATIC_CHECK(transaction.byteEnables == ByteEnables{});
    LUNATIC_CHECK(!transaction.Complete(COMPLETE_WRITE_DATA));

    transaction.Mark(COMPLETE_WRITE_DATA);
    transaction.Mark(COMPLETE_MEMORY);
    LUNATIC_CHECK(transaction.Complete(COMPLETE_WRITE_DATA));
    LUNATIC_CHECK(transaction.Complete(COMPLETE_MEMORY));
    LUNATIC_CHECK(!transaction.Complete(COMPLETE_SNOOPS));
}

LUNATIC_TEST(Stage2TrackerExhaustionReuseAndClear)
{
    Tracker zeroCapacity(0);
    Request request;
    request.source = 3;
    request.txnId = 10;
    LUNATIC_CHECK(zeroCapacity.Validate());
    LUNATIC_CHECK(!zeroCapacity.Allocate(request, 0));

    Tracker tracker(2);
    Transaction* first = tracker.Allocate(request, 5);
    LUNATIC_CHECK(first != nullptr);
    LUNATIC_CHECK(first->handle == 0);
    LUNATIC_CHECK(first->actionAge == 5);
    LUNATIC_CHECK(tracker.Size() == 1);
    LUNATIC_CHECK(!tracker.Empty());
    LUNATIC_CHECK(tracker.Limit() == 2);

    Request duplicate = request;
    duplicate.address = 0x4000;
    LUNATIC_CHECK(!tracker.Allocate(duplicate, 6));
    LUNATIC_CHECK(tracker.Size() == 1);

    request.txnId = 11;
    Transaction* second = tracker.Allocate(request, 6);
    LUNATIC_CHECK(second != nullptr);
    LUNATIC_CHECK(second->handle == 1);
    LUNATIC_CHECK((tracker.Handles() == std::vector<TransactionHandle>{ 0, 1 }));

    request.txnId = 12;
    LUNATIC_CHECK(!tracker.Allocate(request, 7));
    LUNATIC_CHECK(tracker.Size() == 2);
    LUNATIC_CHECK(tracker.Validate());

    LUNATIC_CHECK(tracker.Retire(first->handle));
    Transaction* replacement = tracker.Allocate(duplicate, 8);
    LUNATIC_CHECK(replacement != nullptr);
    LUNATIC_CHECK(replacement->handle == 0);
    LUNATIC_CHECK(tracker.FindRequest(3, 10) == replacement);
    LUNATIC_CHECK(tracker.FindRequest(3, 11) == second);
    LUNATIC_CHECK(tracker.Find(0) == replacement);
    LUNATIC_CHECK(!tracker.Allocate(request, 9));
    LUNATIC_CHECK((tracker.Handles() == std::vector<TransactionHandle>{ 0, 1 }));
    LUNATIC_CHECK(tracker.Validate());

    tracker.Clear();
    LUNATIC_CHECK(tracker.Empty());
    LUNATIC_CHECK(tracker.Size() == 0);
    LUNATIC_CHECK(tracker.Handles().empty());
    LUNATIC_CHECK(!tracker.FindRequest(3, 10));
    LUNATIC_CHECK(tracker.Validate());

    Transaction* afterClear = tracker.Allocate(duplicate, 10);
    LUNATIC_CHECK(afterClear != nullptr);
    LUNATIC_CHECK(afterClear->handle == 0);
    LUNATIC_CHECK(tracker.Validate());
}

LUNATIC_TEST(Stage2TrackerDirectMapsRejectDuplicateLiveIds)
{
    Tracker tracker(8);
    Request firstRequest;
    firstRequest.source = 4;
    firstRequest.txnId = 1;
    Request secondRequest = firstRequest;
    secondRequest.txnId = 2;
    Request otherSourceRequest = firstRequest;
    otherSourceRequest.source = 5;
    otherSourceRequest.txnId = 3;

    Transaction* first = tracker.Allocate(firstRequest, 1);
    Transaction* second = tracker.Allocate(secondRequest, 2);
    Transaction* otherSource = tracker.Allocate(otherSourceRequest, 3);
    LUNATIC_CHECK(first && second && otherSource);

    LUNATIC_CHECK(tracker.BindDBID(first->handle, 77));
    LUNATIC_CHECK(first->dbid == 77);
    LUNATIC_CHECK(!tracker.BindDBID(first->handle, 77));
    LUNATIC_CHECK(!tracker.BindDBID(first->handle, 78));
    LUNATIC_CHECK(!tracker.BindDBID(second->handle, 77));
    LUNATIC_CHECK(tracker.BindDBID(otherSource->handle, 77));
    LUNATIC_CHECK(tracker.FindDBID(4, 77) == first);
    LUNATIC_CHECK(tracker.FindDBID(5, 77) == otherSource);
    LUNATIC_CHECK(!tracker.FindDBID(4, 78));

    LUNATIC_CHECK(tracker.BindSnoop(first->handle, 9, 100));
    LUNATIC_CHECK(!tracker.BindSnoop(first->handle, 9, 100));
    LUNATIC_CHECK(!tracker.BindSnoop(second->handle, 9, 100));
    LUNATIC_CHECK(tracker.BindSnoop(second->handle, 10, 100));
    LUNATIC_CHECK(tracker.BindSnoop(first->handle, 9, 101));
    LUNATIC_CHECK(tracker.FindSnoop(9, 100) == first);
    LUNATIC_CHECK(tracker.FindSnoop(10, 100) == second);
    LUNATIC_CHECK(tracker.FindSnoop(9, 101) == first);
    LUNATIC_CHECK(!tracker.FindSnoop(11, 100));

    LUNATIC_CHECK(tracker.BindMemory(first->handle, 12, 200));
    LUNATIC_CHECK(!tracker.BindMemory(first->handle, 12, 200));
    LUNATIC_CHECK(!tracker.BindMemory(second->handle, 12, 200));
    LUNATIC_CHECK(tracker.BindMemory(second->handle, 13, 200));
    LUNATIC_CHECK(tracker.BindMemory(first->handle, 12, 201));
    LUNATIC_CHECK(tracker.FindMemory(12, 200) == first);
    LUNATIC_CHECK(tracker.FindMemory(13, 200) == second);
    LUNATIC_CHECK(tracker.FindMemory(12, 201) == first);
    LUNATIC_CHECK(!tracker.FindMemory(14, 200));

    LUNATIC_CHECK(!tracker.BindDBID(INVALID_TRANSACTION, 1));
    LUNATIC_CHECK(!tracker.BindSnoop(INVALID_TRANSACTION, 1, 1));
    LUNATIC_CHECK(!tracker.BindMemory(INVALID_TRANSACTION, 1, 1));
    LUNATIC_CHECK(!tracker.Find(INVALID_TRANSACTION));
    LUNATIC_CHECK(tracker.Validate());
}

LUNATIC_TEST(Stage2TrackerRetirementCleansEveryOwnedIndex)
{
    Tracker tracker(2);
    Request request;
    request.source = 6;
    request.txnId = 21;
    Transaction* transaction = tracker.Allocate(request, 4);
    LUNATIC_CHECK(transaction != nullptr);
    const TransactionHandle handle = transaction->handle;

    LUNATIC_CHECK(tracker.BindDBID(handle, 88));
    LUNATIC_CHECK(tracker.BindSnoop(handle, 20, 1));
    LUNATIC_CHECK(tracker.BindSnoop(handle, 21, 1));
    LUNATIC_CHECK(tracker.BindMemory(handle, 30, 2));
    LUNATIC_CHECK(tracker.BindMemory(handle, 31, 2));
    LUNATIC_CHECK(!tracker.Retire(INVALID_TRANSACTION));
    LUNATIC_CHECK(tracker.Validate());

    LUNATIC_CHECK(tracker.Retire(handle));
    LUNATIC_CHECK(!tracker.Retire(handle));
    LUNATIC_CHECK(tracker.Empty());
    LUNATIC_CHECK(!tracker.Find(handle));
    LUNATIC_CHECK(!tracker.FindRequest(6, 21));
    LUNATIC_CHECK(!tracker.FindDBID(6, 88));
    LUNATIC_CHECK(!tracker.FindSnoop(20, 1));
    LUNATIC_CHECK(!tracker.FindSnoop(21, 1));
    LUNATIC_CHECK(!tracker.FindMemory(30, 2));
    LUNATIC_CHECK(!tracker.FindMemory(31, 2));
    LUNATIC_CHECK(tracker.Validate());

    Transaction* replacement = tracker.Allocate(request, 5);
    LUNATIC_CHECK(replacement != nullptr);
    LUNATIC_CHECK(replacement->handle == 1);
    LUNATIC_CHECK(tracker.BindDBID(replacement->handle, 88));
    LUNATIC_CHECK(tracker.BindSnoop(replacement->handle, 20, 1));
    LUNATIC_CHECK(tracker.BindMemory(replacement->handle, 30, 2));
    LUNATIC_CHECK(tracker.Validate());
}

LUNATIC_TEST(Stage2MemoryIdCanBeReleasedWithoutRetiringRequest)
{
    Tracker tracker(3);
    Request firstRequest;
    firstRequest.source = 2;
    firstRequest.txnId = 1;
    Request secondRequest = firstRequest;
    secondRequest.txnId = 2;
    Transaction* first = tracker.Allocate(firstRequest, 1);
    Transaction* second = tracker.Allocate(secondRequest, 2);
    LUNATIC_CHECK(first && second);

    LUNATIC_CHECK(tracker.BindDBID(first->handle, 7));
    LUNATIC_CHECK(tracker.BindSnoop(first->handle, 8, 9));
    LUNATIC_CHECK(tracker.BindMemory(first->handle, 10, 55));
    LUNATIC_CHECK(!tracker.UnbindMemory(10, 54));
    LUNATIC_CHECK(tracker.FindMemory(10, 55) == first);
    LUNATIC_CHECK(tracker.UnbindMemory(10, 55));
    LUNATIC_CHECK(!tracker.UnbindMemory(10, 55));
    LUNATIC_CHECK(!tracker.FindMemory(10, 55));
    LUNATIC_CHECK(tracker.FindRequest(2, 1) == first);
    LUNATIC_CHECK(tracker.FindDBID(2, 7) == first);
    LUNATIC_CHECK(tracker.FindSnoop(8, 9) == first);

    LUNATIC_CHECK(tracker.BindMemory(second->handle, 10, 55));
    LUNATIC_CHECK(tracker.FindMemory(10, 55) == second);
    LUNATIC_CHECK(tracker.Retire(first->handle));
    LUNATIC_CHECK(tracker.FindMemory(10, 55) == second);
    LUNATIC_CHECK(!tracker.FindRequest(2, 1));
    LUNATIC_CHECK(!tracker.FindDBID(2, 7));
    LUNATIC_CHECK(!tracker.FindSnoop(8, 9));
    LUNATIC_CHECK(tracker.Validate());

    LUNATIC_CHECK(tracker.Retire(second->handle));
    LUNATIC_CHECK(!tracker.FindMemory(10, 55));
    LUNATIC_CHECK(tracker.Validate());
}

LUNATIC_TEST(Stage2HazardAgeOrderAndHeadOnlyRelease)
{
    HazardGraph graph;
    const LineKey line = LineKey::FromAddress(0x1234, false);
    const LineKey otherLine = LineKey::FromAddress(0x2234, false);
    LUNATIC_CHECK(graph.Empty());
    LUNATIC_CHECK(graph.Validate());
    LUNATIC_CHECK(!graph.Has(line));
    LUNATIC_CHECK(!graph.Next(line));
    LUNATIC_CHECK(graph.Order(line).empty());
    LUNATIC_CHECK(!graph.IsHead(1, line));
    LUNATIC_CHECK(!graph.Release(1, line));

    LUNATIC_CHECK(graph.Add(8, line, 20));
    LUNATIC_CHECK(graph.Add(3, line, 10));
    LUNATIC_CHECK(graph.Add(5, line, 10));
    LUNATIC_CHECK(graph.Add(2, otherLine, 0));
    LUNATIC_CHECK(!graph.Add(3, line, 1));
    LUNATIC_CHECK(!graph.Add(INVALID_TRANSACTION, line, 0));
    LUNATIC_CHECK(!graph.Empty());
    LUNATIC_CHECK(graph.Has(line));
    LUNATIC_CHECK(graph.Has(otherLine));
    LUNATIC_CHECK(graph.Next(line) == std::optional<TransactionHandle>(3));
    LUNATIC_CHECK(graph.Next(otherLine) == std::optional<TransactionHandle>(2));
    LUNATIC_CHECK(graph.IsHead(3, line));
    LUNATIC_CHECK(!graph.IsHead(5, line));
    LUNATIC_CHECK((graph.Order(line) == std::vector<TransactionHandle>{ 3, 5, 8 }));
    LUNATIC_CHECK(graph.Validate());

    LUNATIC_CHECK(!graph.Release(5, line));
    LUNATIC_CHECK(!graph.Release(3, otherLine));
    LUNATIC_CHECK(graph.Release(3, line));
    LUNATIC_CHECK(!graph.Release(3, line));
    LUNATIC_CHECK(graph.Next(line) == std::optional<TransactionHandle>(5));
    LUNATIC_CHECK(graph.Release(5, line));
    LUNATIC_CHECK(graph.Release(8, line));
    LUNATIC_CHECK(!graph.Has(line));
    LUNATIC_CHECK(graph.Has(otherLine));
    LUNATIC_CHECK(graph.Validate());

    graph.Clear();
    LUNATIC_CHECK(graph.Empty());
    LUNATIC_CHECK(!graph.Has(otherLine));
    LUNATIC_CHECK(graph.Validate());
}

LUNATIC_TEST(Stage2ArbiterUsesDeterministicAgeOrder)
{
    Arbiter arbiter;
    LUNATIC_CHECK(arbiter.Size() == 0);
    LUNATIC_CHECK(!arbiter.Peek());
    LUNATIC_CHECK(!arbiter.PeekQoS());
    LUNATIC_CHECK(!arbiter.Remove(1));

    LUNATIC_CHECK(!arbiter.Submit(INVALID_TRANSACTION, 0));
    LUNATIC_CHECK(arbiter.Submit(9, 30, 15));
    LUNATIC_CHECK(arbiter.Submit(4, 10, 1));
    LUNATIC_CHECK(arbiter.Submit(7, 10, 2));
    LUNATIC_CHECK(!arbiter.Submit(4, 0, 15));
    LUNATIC_CHECK(arbiter.Size() == 3);
    LUNATIC_CHECK(arbiter.Peek() == std::optional<TransactionHandle>(4));
    LUNATIC_CHECK(arbiter.PeekQoS() == std::optional<TransactionHandle>(9));

    LUNATIC_CHECK(arbiter.Remove(4));
    LUNATIC_CHECK(!arbiter.Remove(4));
    LUNATIC_CHECK(arbiter.Peek() == std::optional<TransactionHandle>(7));
    LUNATIC_CHECK(arbiter.Remove(7));
    LUNATIC_CHECK(arbiter.Peek() == std::optional<TransactionHandle>(9));
    arbiter.Clear();
    LUNATIC_CHECK(arbiter.Size() == 0);
    LUNATIC_CHECK(!arbiter.Peek());
}

LUNATIC_TEST(Stage2ArbiterQoSThenAgeAndHandle)
{
    Arbiter arbiter;
    LUNATIC_CHECK(arbiter.Submit(1, 5, 1));
    LUNATIC_CHECK(arbiter.Submit(2, 9, 9));
    LUNATIC_CHECK(arbiter.Submit(4, 7, 9));
    LUNATIC_CHECK(arbiter.Submit(3, 7, 9));

    LUNATIC_CHECK(arbiter.Peek() == std::optional<TransactionHandle>(1));
    LUNATIC_CHECK(arbiter.PeekQoS() == std::optional<TransactionHandle>(3));
    LUNATIC_CHECK(arbiter.Remove(3));
    LUNATIC_CHECK(arbiter.PeekQoS() == std::optional<TransactionHandle>(4));
    LUNATIC_CHECK(arbiter.Remove(4));
    LUNATIC_CHECK(arbiter.PeekQoS() == std::optional<TransactionHandle>(2));
    LUNATIC_CHECK(arbiter.Remove(2));
    LUNATIC_CHECK(arbiter.PeekQoS() == std::optional<TransactionHandle>(1));
}

LUNATIC_TEST(Stage2SparseMemoryDefaultFullWritesAndSecurityNamespace)
{
    SparseMemory memory;
    const LineKey key = LineKey::FromAddress(0x1047, false);
    const LineKey secureKey = LineKey::FromAddress(0x1047, true);
    LUNATIC_CHECK(key.address == 0x1040);
    LUNATIC_CHECK(memory.Size() == 0);
    LUNATIC_CHECK(!memory.Contains(key));
    LUNATIC_CHECK(memory.Read(key) == LineData{});

    LineData initial{};
    for (size_t index = 0; index < LINE_SIZE; ++index)
        initial[index] = uint8_t(index + 1);
    memory.WriteFull(key, initial);
    LUNATIC_CHECK(memory.Contains(key));
    LUNATIC_CHECK(memory.Size() == 1);
    LUNATIC_CHECK(memory.Read(key) == initial);
    LUNATIC_CHECK(!memory.Contains(secureKey));
    LUNATIC_CHECK(memory.Read(secureKey) == LineData{});

    LineData replacement{};
    replacement.fill(0xA5);
    memory.WriteFull(key, replacement);
    LUNATIC_CHECK(memory.Read(key) == replacement);
    LUNATIC_CHECK(memory.Size() == 1);

    memory.Erase(key);
    memory.Erase(key);
    LUNATIC_CHECK(!memory.Contains(key));
    LUNATIC_CHECK(memory.Read(key) == LineData{});
    LUNATIC_CHECK(memory.Size() == 0);
}

LUNATIC_TEST(Stage2SparseMemoryByteEnableMerge)
{
    SparseMemory memory;
    const LineKey key = LineKey::FromAddress(0x2000, false);
    LineData initial{};
    for (size_t index = 0; index < LINE_SIZE; ++index)
        initial[index] = uint8_t(0x10 + index);
    memory.WriteFull(key, initial);

    LineData update{};
    for (size_t index = 0; index < LINE_SIZE; ++index)
        update[index] = uint8_t(0x80 + index);
    ByteEnables enables{};
    enables[0] = 1;
    enables[7] = 2;
    enables[31] = 1;
    enables[63] = 0xFF;
    memory.Write(key, update, enables);

    LineData expected = initial;
    expected[0] = update[0];
    expected[7] = update[7];
    expected[31] = update[31];
    expected[63] = update[63];
    LUNATIC_CHECK(memory.Read(key) == expected);
    LUNATIC_CHECK(memory.Size() == 1);

    const LineKey partialKey = LineKey::FromAddress(0x3000, true);
    ByteEnables partialEnables{};
    partialEnables[5] = 1;
    partialEnables[63] = 1;
    memory.Write(partialKey, update, partialEnables);
    LineData partialExpected{};
    partialExpected[5] = update[5];
    partialExpected[63] = update[63];
    LUNATIC_CHECK(memory.Contains(partialKey));
    LUNATIC_CHECK(memory.Read(partialKey) == partialExpected);
    LUNATIC_CHECK(memory.Read(key) == expected);
    LUNATIC_CHECK(memory.Size() == 2);

    memory.Clear();
    LUNATIC_CHECK(memory.Size() == 0);
    LUNATIC_CHECK(memory.Read(key) == LineData{});
    LUNATIC_CHECK(memory.Read(partialKey) == LineData{});
}

LUNATIC_TEST(Stage2SparseMemoryLatencyScriptIsFifoAndFinite)
{
    SparseMemory memory;
    LUNATIC_CHECK(memory.NextLatency(11) == 11);
    LUNATIC_CHECK(memory.NextLatency(12) == 12);

    memory.ScriptLatency(4);
    memory.ScriptLatency(0);
    memory.ScriptLatency(9);
    LUNATIC_CHECK(memory.NextLatency(99) == 4);
    LUNATIC_CHECK(memory.NextLatency(99) == 0);
    LUNATIC_CHECK(memory.NextLatency(99) == 9);
    LUNATIC_CHECK(memory.NextLatency(99) == 99);
    LUNATIC_CHECK(memory.NextLatency(13) == 13);

    memory.ScriptLatency(6);
    memory.Clear();
    LUNATIC_CHECK(memory.NextLatency(21) == 21);
}
