#include "timing_matrix.hpp"

#include <array>
#include <map>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;
using namespace LunaticTest::Timing;

namespace {

    constexpr std::array<ProfileKind, 2> WriteProfiles = { ProfileKind::Type3, ProfileKind::Type7 };

    enum class DependencyPosition : uint8_t {
        Early,
        Same,
        Late
    };

    constexpr std::array<DependencyPosition, 3> DependencyPositions = {
        DependencyPosition::Early, DependencyPosition::Same, DependencyPosition::Late
    };

    Time DependencyOffset(DependencyPosition position)
    {
        return position == DependencyPosition::Early ? 0 : position == DependencyPosition::Same ? 1 : 5;
    }

    Time PlaceDependency(CoreDriver& driver, DependencyPosition position, Time boundary)
    {
        const Time accepted = boundary + DependencyOffset(position);
        driver.TickTo(accepted);
        return accepted;
    }

    LineData WritePattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 13);
        return data;
    }

    ByteEnables FullWriteEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    ByteEnables SparseWriteEnables()
    {
        ByteEnables enables{};
        for (size_t index = 5; index < 13; ++index)
            enables[index] = 1;
        for (size_t index = 40; index < 46; ++index)
            enables[index] = 1;
        return enables;
    }

    Request WriteRequest(uint16_t opcode, TransactionId txnId = 1, uint64_t address = 0x100000)
    {
        return Timing::MakeRequest(opcode, 4, txnId, address);
    }

    Transaction& AddBoundWrite(const Row& row, CoreDriver& driver, const Request& request)
    {
        Transaction& transaction = driver.Add(request);
        TIMING_CHECK(row, driver.impl.tracker.BindDBID(transaction.handle, transaction.dbid));
        return transaction;
    }

    void SeedHome(CoreDriver& driver, Transaction& transaction, const LineData& data, bool dirty)
    {
        Core::Impl& impl = driver.impl;
        const LineKey line = transaction.request.Line();
        HomeLine& home = impl.InstallHome(line, 0, data, dirty, impl.time);
        transaction.homeHit = true;
        transaction.data = data;
        FlowState& flow = impl.flows.at(transaction.handle);
        flow.homeDirty = dirty;
        flow.validBytes = home.validBytes;
    }

    const Pending<Response>& FindResponse(Core::Impl& impl, uint16_t opcode)
    {
        for (const Pending<Response>& pending : impl.txrsp)
            if (pending.flit.opcode == opcode)
                return pending;
        throw std::runtime_error("expected upstream response was not queued");
    }

    void ExpectResponseTime(const Row& row, Core::Impl& impl, uint16_t opcode, Time expected)
    {
        const Pending<Response>& pending = FindResponse(impl, opcode);
        ExpectTime(row, pending.readyTime, expected, "response visibility");
        TIMING_CHECK(row, pending.readyTime > impl.time || impl.Peek(impl.txrsp) != nullptr);
    }

    Response PopResponse(const Row& row, CoreDriver& driver, uint16_t opcode, Time visible)
    {
        Core::Impl& impl = driver.impl;
        driver.TickTo(visible);
        const auto response = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
        TIMING_CHECK(row, response.has_value());
        TIMING_CHECK(row, response->opcode == opcode);
        return *response;
    }

    Request PopBackendRequest(const Row& row, CoreDriver& driver, Time visible, uint16_t opcode)
    {
        Core::Impl& impl = driver.impl;
        driver.TickTo(visible);
        const auto request = impl.Pop(impl.txreq, EventKind::EmittedTXREQ);
        TIMING_CHECK(row, request.has_value());
        TIMING_CHECK(row, request->opcode == opcode);
        return *request;
    }

    Data PopBackendData(const Row& row, CoreDriver& driver, Time visible, uint8_t dataId)
    {
        Core::Impl& impl = driver.impl;
        driver.TickTo(visible);
        const auto data = impl.Pop(impl.txdat, EventKind::EmittedTXDAT);
        TIMING_CHECK(row, data.has_value());
        TIMING_CHECK(row, data->dataId == dataId);
        return *data;
    }

    Snoop PopSnoop(const Row& row, CoreDriver& driver, Time visible, uint16_t opcode)
    {
        Core::Impl& impl = driver.impl;
        driver.TickTo(visible);
        const auto snoop = impl.Pop(impl.txsnp, EventKind::EmittedTXSNP);
        TIMING_CHECK(row, snoop.has_value());
        TIMING_CHECK(row, snoop->opcode == opcode);
        return *snoop;
    }

    Response BackendResponse(const Transaction& transaction, uint16_t opcode, DataBufferId dbid = 77)
    {
        Response response;
        response.source = 6;
        response.target = 2;
        response.txnId = transaction.handle;
        response.dbid = dbid;
        response.opcode = opcode;
        return response;
    }

    void SendWriteData(const Row& row, CoreDriver& driver, Transaction& transaction,
                       const LineData& payload, const ByteEnables& enables,
                       uint16_t opcode = CHI::Eb::Opcodes::DAT::NonCopyBackWrData, uint8_t resp = 0)
    {
        Core::Impl& impl = driver.impl;
        const FlowState& flow = impl.flows.at(transaction.handle);
        const size_t bytes = BytesPerBeat(impl.options);
        for (size_t beat = 0; beat < BeatsPerLine(impl.options); ++beat)
        {
            if (!(flow.expectedBeatMask & (uint64_t(1) << beat)))
                continue;
            Data data;
            data.source = transaction.request.source;
            data.target = 2;
            data.homeNid = 2;
            data.txnId = transaction.dbid;
            data.opcode = opcode;
            data.resp = resp;
            data.dataId = beat * bytes / 16;
            data.ccid = (beat * bytes / 16) % 2;
            for (size_t index = 0; index < bytes; ++index)
            {
                data.bytes[index] = payload[beat * bytes + index];
                data.byteEnables[index] = enables[beat * bytes + index];
            }
            TIMING_CHECK(row, impl.HandleWriteDAT(transaction, data));
        }
    }

    void SendBackendData(const Row& row, CoreDriver& driver, Transaction& transaction,
                         const LineData& payload)
    {
        Core::Impl& impl = driver.impl;
        const size_t bytes = BytesPerBeat(impl.options);
        for (size_t beat = 0; beat < BeatsPerLine(impl.options); ++beat)
        {
            Data data;
            data.source = 6;
            data.target = 2;
            data.homeNid = 2;
            data.txnId = transaction.handle;
            data.opcode = CHI::Eb::Opcodes::DAT::CompData;
            data.dataId = beat * bytes / 16;
            data.ccid = (transaction.request.address >> 4) & 3;
            for (size_t index = 0; index < bytes; ++index)
            {
                data.bytes[index] = payload[beat * bytes + index];
                data.byteEnables[index] = 1;
            }
            TIMING_CHECK(row, impl.HandleBackendDAT(transaction, data));
        }
    }

    void SendSnoopResponse(const Row& row, CoreDriver& driver, Transaction& transaction,
                           NodeId source, TransactionId txnId, bool withData, const LineData& payload)
    {
        Core::Impl& impl = driver.impl;
        if (!withData)
        {
            Response response;
            response.source = source;
            response.target = 2;
            response.txnId = txnId;
            response.opcode = CHI::Eb::Opcodes::RSP::SnpResp;
            response.resp = CHI::Eb::Resps::SnpResp_I;
            impl.ProcessSnoopRSP(transaction, response);
            return;
        }
        const size_t bytes = BytesPerBeat(impl.options);
        for (size_t beat = 0; beat < BeatsPerLine(impl.options); ++beat)
        {
            Data data;
            data.source = source;
            data.target = 2;
            data.homeNid = 2;
            data.txnId = txnId;
            data.opcode = CHI::Eb::Opcodes::DAT::SnpRespData;
            data.resp = CHI::Eb::Resps::SnpRespData_I_PD;
            data.dataId = beat * bytes / 16;
            for (size_t index = 0; index < bytes; ++index)
            {
                data.bytes[index] = payload[beat * bytes + index];
                data.byteEnables[index] = 1;
            }
            impl.ProcessSnoopDAT(transaction, data);
        }
    }

    void SendCompAck(CoreDriver& driver, Transaction& transaction)
    {
        Response response;
        response.source = transaction.request.source;
        response.target = 2;
        response.txnId = transaction.dbid;
        response.opcode = CHI::Eb::Opcodes::RSP::CompAck;
        driver.impl.ProcessRSP(response);
    }

    void SendCancel(const Row& row, CoreDriver& driver, Transaction& transaction)
    {
        Data cancel;
        cancel.source = transaction.request.source;
        cancel.target = 2;
        cancel.txnId = transaction.dbid;
        cancel.opcode = CHI::Eb::Opcodes::DAT::WriteDataCancel;
        TIMING_CHECK(row, driver.impl.HandleWriteDAT(transaction, cancel));
    }

    Time CompleteInternalNonCoherent(const Row& row, CoreDriver& driver, Transaction& transaction,
                                     Time dataAccepted)
    {
        Core::Impl& impl = driver.impl;
        FlowState& flow = impl.flows.at(transaction.handle);
        if (flow.backendPhase == BackendPhase::None)
            impl.ActionNonCoherent(transaction);
        TIMING_CHECK(row, flow.memoryWrite);
        ExpectTime(row, flow.memoryReady, dataAccepted + impl.options.memoryLatency, "internal write readiness");
        const Time action = flow.memoryReady + impl.options.dataLatency + impl.options.timing.dataReturn;
        driver.TickTo(action);
        TIMING_CHECK(row, flow.backendPhase == BackendPhase::Done);
        return action;
    }

    void CheckClean(const Row& row, CoreDriver& driver)
    {
        TIMING_CHECK(row, driver.impl.errors.empty());
        TIMING_CHECK(row, driver.impl.ValidateInvariants());
        TIMING_CHECK(row, !driver.impl.HasInflight());
    }

}

LUNATIC_TEST(Stage10MatrixW01InternalWriteDataDependencyTiming)
{
    for (const ProfileKind profile : WriteProfiles)
        for (const DependencyPosition position : DependencyPositions)
        {
            const Row row{ profile, "W01" };
            Options options = ProfileOptions(profile);
            options.memoryLatency = 4;
            options.timing.dataReturn = 1;
            options.timing.fastResponse = 2;
            options.timing.normalResponse = 3;
            options.timing.output = 1;
            options.timing.commitSchedule = 2;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            impl.time = 10;
            Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(CHI::Eb::Opcodes::REQ::WriteNoSnpFull));
            const TransactionHandle handle = transaction.handle;
            const LineKey line = transaction.request.Line();
            const LineData payload = WritePattern(11);
            impl.ResolveNonCoherent(transaction);
            impl.ActionNonCoherent(transaction);
            ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::DBIDResp, 13);
            const Response dbid = PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 13);
            TIMING_CHECK(row, dbid.dbid == transaction.dbid);
            const Time accepted = PlaceDependency(driver, position, 13);
            SendWriteData(row, driver, transaction, payload, FullWriteEnables());
            TIMING_CHECK(row, transaction.Complete(COMPLETE_WRITE_DATA));
            const Time action = CompleteInternalNonCoherent(row, driver, transaction, accepted);
            ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::Comp, action + 4);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, action + 4);
            TIMING_CHECK(row, !impl.flows.at(handle).responseOwned);
            impl.RetireFinished();
            driver.TickTo(action + 5);
            TIMING_CHECK(row, impl.tracker.Find(handle) != nullptr);
            driver.TickTo(action + 6);
            TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
            TIMING_CHECK(row, impl.memory.Read(line) == payload);
            CheckClean(row, driver);
        }
}

LUNATIC_TEST(Stage10MatrixW02ChiBackendResponseAndDataTiming)
{
    for (const ProfileKind profile : WriteProfiles)
        for (const DependencyPosition position : DependencyPositions)
        {
            const Row row{ profile, "W02" };
            Options options = ProfileOptions(profile);
            options.backend = BackendKind::CHI;
            options.subordinateId = 6;
            options.timing.fastResponse = 2;
            options.timing.normalResponse = 3;
            options.timing.backendRequest = 2;
            options.timing.backendData = 3;
            options.timing.dataBeat = 2;
            options.timing.output = 1;
            options.timing.commitSchedule = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            impl.time = 20;
            Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(CHI::Eb::Opcodes::REQ::WriteNoSnpFull));
            const TransactionHandle handle = transaction.handle;
            impl.ResolveNonCoherent(transaction);
            impl.ActionNonCoherent(transaction);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 23);
            SendWriteData(row, driver, transaction, WritePattern(23), FullWriteEnables());
            impl.ActionNonCoherent(transaction);
            TIMING_CHECK(row, impl.txreq.size() == 1);
            ExpectTime(row, impl.txreq.front().readyTime, 26, "downstream write request");
            PopBackendRequest(row, driver, 26, CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
            const Time dbidAccepted = PlaceDependency(driver, position, 26);
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::DBIDResp)));
            if (position == DependencyPosition::Early)
                TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                    BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
            FlowState& flow = impl.flows.at(handle);
            TIMING_CHECK(row, flow.backendOperationOwned);
            impl.AdvanceBackendWrite(transaction);
            const Time firstVisible = dbidAccepted + 4;
            ExpectTime(row, impl.txdat.front().readyTime, firstVisible, "first downstream write beat");
            impl.AdvanceBackendWrite(transaction);
            const Time secondVisible = dbidAccepted + 6;
            driver.TickTo(firstVisible);
            TIMING_CHECK(row, impl.txdat.size() == 2);
            ExpectTime(row, impl.txdat[0].readyTime, firstVisible, "held first downstream beat");
            ExpectTime(row, impl.txdat[1].readyTime, secondVisible, "second downstream write beat");
            PopBackendData(row, driver, firstVisible, 0);
            if (position == DependencyPosition::Same)
            {
                driver.TickTo(secondVisible);
                TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                    BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
            }
            TIMING_CHECK(row, flow.backendOperationOwned);
            PopBackendData(row, driver, secondVisible, 2);
            if (position == DependencyPosition::Late)
            {
                TIMING_CHECK(row, flow.backendOperationOwned);
                driver.TickTo(secondVisible + 4);
                TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                    BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
            }
            const Time completed = position == DependencyPosition::Late ? secondVisible + 4 : secondVisible;
            TIMING_CHECK(row, !flow.backendOperationOwned);
            impl.ActionNonCoherent(transaction);
            ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::Comp, completed + 4);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, completed + 4);
            impl.RetireFinished();
            driver.TickTo(completed + 5);
            TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
            CheckClean(row, driver);
        }
}

LUNATIC_TEST(Stage10MatrixW03AxiIndependentChannelsAndResponsePositions)
{
    const std::array<uint16_t, 2> opcodes = {
        CHI::Eb::Opcodes::REQ::WriteNoSnpFull, CHI::Eb::Opcodes::REQ::WriteNoSnpPtl
    };
    for (const ProfileKind profile : WriteProfiles)
        for (const uint16_t opcode : opcodes)
            for (const DependencyPosition position : DependencyPositions)
            {
                const Row row{ profile, "W03" };
                Options options = ProfileOptions(profile);
                options.backend = BackendKind::AXI;
                options.timing.fastResponse = 1;
                options.timing.normalResponse = 2;
                options.timing.output = 2;
                options.timing.commitSchedule = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                impl.time = 30;
                Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(opcode));
                const TransactionHandle handle = transaction.handle;
                const ByteEnables enables = opcode == CHI::Eb::Opcodes::REQ::WriteNoSnpFull
                    ? FullWriteEnables() : SparseWriteEnables();
                impl.ResolveNonCoherent(transaction);
                impl.ActionNonCoherent(transaction);
                PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 33);
                SendWriteData(row, driver, transaction, WritePattern(37), enables);
                impl.ActionNonCoherent(transaction);
                TIMING_CHECK(row, impl.axi.PeekAW().has_value());
                TIMING_CHECK(row, !impl.axi.PeekW().has_value());
                driver.TickTo(34);
                const auto aw = impl.axi.PopAW();
                TIMING_CHECK(row, aw.has_value());
                TIMING_CHECK(row, aw->len == 1);
                TIMING_CHECK(row, impl.tracker.Find(handle) != nullptr);
                driver.TickTo(35);
                const auto first = impl.axi.PopW();
                TIMING_CHECK(row, first.has_value());
                TIMING_CHECK(row, !first->last);
                const Time lastAccepted = PlaceDependency(driver, position, 35);
                const auto last = impl.axi.PopW();
                TIMING_CHECK(row, last.has_value());
                TIMING_CHECK(row, last->last);
                for (size_t beat = 0; beat < 2; ++beat)
                {
                    const AXIWriteData& data = beat == 0 ? *first : *last;
                    for (size_t index = 0; index < BytesPerBeat(options); ++index)
                        TIMING_CHECK(row, data.strobes[index] == enables[beat * BytesPerBeat(options) + index]);
                }
                TIMING_CHECK(row, impl.txrsp.empty());
                TIMING_CHECK(row, impl.flows.at(handle).backendOperationOwned);
                const Time responseAccepted = PlaceDependency(driver, position, lastAccepted);
                AXIWriteResponse response;
                response.id = aw->id;
                TIMING_CHECK(row, impl.axi.PushB(response));
                impl.Tick();
                TIMING_CHECK(row, !impl.flows.at(handle).backendOperationOwned);
                ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::Comp, responseAccepted + 4);
                PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, responseAccepted + 4);
                impl.RetireFinished();
                driver.TickTo(responseAccepted + 5);
                TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
                TIMING_CHECK(row, !impl.axi.HasInflight());
                TIMING_CHECK(row, impl.txreq.empty() && impl.txdat.empty());
                CheckClean(row, driver);
            }
}

LUNATIC_TEST(Stage10MatrixW04SparsePartialAndZeroTransferMasks)
{
    for (const ProfileKind profile : WriteProfiles)
    {
        const Row row{ profile, "W04" };
        Options options = ProfileOptions(profile);
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.timing.fastResponse = 1;
        options.timing.normalResponse = 2;
        options.timing.backendRequest = 1;
        options.timing.backendData = 2;
        options.timing.dataBeat = 3;
        options.timing.output = 1;
        options.timing.commitSchedule = 1;
        const ByteEnables sparse = SparseWriteEnables();
        for (const DependencyPosition position : DependencyPositions)
        {
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            impl.time = 40;
            Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl));
            const TransactionHandle handle = transaction.handle;
            impl.ResolveNonCoherent(transaction);
            impl.ActionNonCoherent(transaction);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 42);
            const Time accepted = PlaceDependency(driver, position, 42);
            SendWriteData(row, driver, transaction, WritePattern(41), sparse);
            impl.ActionNonCoherent(transaction);
            const Time requestVisible = accepted + 2;
            PopBackendRequest(row, driver, requestVisible, CHI::Eb::Opcodes::REQ::WriteNoSnpPtl);
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::DBIDResp)));
            impl.AdvanceBackendWrite(transaction);
            const Time firstVisible = requestVisible + 3;
            ExpectTime(row, impl.txdat.front().readyTime, firstVisible, "sparse first beat");
            impl.AdvanceBackendWrite(transaction);
            const Time secondVisible = requestVisible + 6;
            driver.TickTo(firstVisible);
            TIMING_CHECK(row, impl.txdat.size() == 2);
            ExpectTime(row, impl.txdat[1].readyTime, secondVisible, "sparse beat interval");
            const Time beatMark = impl.flows.at(handle).scheduleMarks.at(SchedulePoint::DataBeat);
            for (size_t beat = 0; beat < 2; ++beat)
            {
                const Pending<Data>& pending = impl.txdat[beat];
                for (size_t index = 0; index < BytesPerBeat(options); ++index)
                {
                    TIMING_CHECK(row, pending.flit.byteEnables[index] == sparse[beat * 32 + index]);
                    if (!sparse[beat * 32 + index])
                        TIMING_CHECK(row, pending.flit.bytes[index] == 0);
                }
            }
            PopBackendData(row, driver, firstVisible, 0);
            driver.TickTo(firstVisible + 1);
            ExpectTime(row, impl.flows.at(handle).scheduleMarks.at(SchedulePoint::DataBeat), beatMark,
                "latched sparse beat interval");
            PopBackendData(row, driver, secondVisible, 2);
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
            impl.ActionNonCoherent(transaction);
            const Time compVisible = secondVisible + 3;
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, compVisible);
            impl.RetireFinished();
            driver.TickTo(compVisible + 1);
            TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
            CheckClean(row, driver);
        }
        {
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            impl.time = 45;
            Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(CHI::Eb::Opcodes::REQ::WriteNoSnpZero));
            const TransactionHandle handle = transaction.handle;
            impl.ResolveNonCoherent(transaction);
            TIMING_CHECK(row, transaction.phase == Phase::WaitMemory);
            TIMING_CHECK(row, transaction.data == LineData{});
            TIMING_CHECK(row, transaction.byteEnables == FullWriteEnables());
            TIMING_CHECK(row, impl.rxdat.empty() && impl.txdat.empty());
            PopBackendRequest(row, driver, 47, CHI::Eb::Opcodes::REQ::WriteNoSnpZero);
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::CompDBIDResp)));
            TIMING_CHECK(row, !impl.flows.at(handle).backendOperationOwned);
            impl.ActionNonCoherent(transaction);
            ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::CompDBIDResp, 49);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::CompDBIDResp, 49);
            impl.RetireFinished();
            driver.TickTo(50);
            TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
            TIMING_CHECK(row, impl.txdat.empty());
            CheckClean(row, driver);
        }
    }
}

LUNATIC_TEST(Stage10MatrixW05OrderedWritesAndOwoCompAckPositions)
{
    const std::array<uint16_t, 2> opcodes = {
        CHI::Eb::Opcodes::REQ::WriteNoSnpFull, CHI::Eb::Opcodes::REQ::WriteUniqueFull
    };
    for (const ProfileKind profile : WriteProfiles)
        for (const uint16_t opcode : opcodes)
            for (const DependencyPosition position : DependencyPositions)
            {
                const Row row{ profile, "W05" };
                Options options = ProfileOptions(profile);
                options.memoryLatency = 2;
                options.timing.fastResponse = 2;
                options.timing.normalResponse = 4;
                options.timing.output = 1;
                options.timing.commitSchedule = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                impl.time = 50;
                Request request = WriteRequest(opcode);
                request.order = 2;
                request.expCompAck = true;
                Transaction& transaction = AddBoundWrite(row, driver, request);
                const TransactionHandle handle = transaction.handle;
                const DataBufferId dbid = transaction.dbid;
                if (opcode == CHI::Eb::Opcodes::REQ::WriteNoSnpFull)
                {
                    impl.ResolveNonCoherent(transaction);
                    impl.ActionNonCoherent(transaction);
                }
                else
                {
                    impl.ResolveImmediateWrite(transaction);
                    impl.QueueImmediateResponse(transaction);
                }
                TIMING_CHECK(row, !impl.ordering.Empty());
                ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::DBIDRespOrd, 53);
                PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDRespOrd, 53);
                TIMING_CHECK(row, !impl.ordering.Empty());
                TIMING_CHECK(row, impl.tracker.FindDBID(4, dbid) == &transaction);
                if (position == DependencyPosition::Early)
                {
                    SendCompAck(driver, transaction);
                    TIMING_CHECK(row, transaction.Complete(COMPLETE_COMPACK));
                    TIMING_CHECK(row, impl.ordering.Empty());
                }
                SendWriteData(row, driver, transaction, WritePattern(53), FullWriteEnables());
                Time action = 53;
                if (opcode == CHI::Eb::Opcodes::REQ::WriteNoSnpFull)
                    action = CompleteInternalNonCoherent(row, driver, transaction, 53);
                else
                {
                    impl.ActionImmediateWrite(transaction);
                    TIMING_CHECK(row, impl.flows.at(handle).directoryCommitted);
                }
                const Time compVisible = opcode == CHI::Eb::Opcodes::REQ::WriteNoSnpFull ? action + 5 : 55;
                ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::Comp, compVisible);
                const Time compPopped = impl.time > compVisible ? impl.time : compVisible;
                PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, compPopped);
                Time completed = compPopped;
                if (position != DependencyPosition::Early)
                {
                    TIMING_CHECK(row, transaction.phase == Phase::WaitCompAck);
                    TIMING_CHECK(row, impl.flows.at(handle).responseOwned);
                    completed = PlaceDependency(driver, position, compPopped);
                    SendCompAck(driver, transaction);
                }
                TIMING_CHECK(row, transaction.Complete(COMPLETE_COMPACK));
                TIMING_CHECK(row, impl.ordering.Empty());
                TIMING_CHECK(row, !impl.flows.at(handle).responseOwned);
                impl.RetireFinished();
                driver.TickTo(completed + 1);
                TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
                TIMING_CHECK(row, impl.tracker.FindDBID(4, dbid) == nullptr);
                CheckClean(row, driver);
            }
}

LUNATIC_TEST(Stage10MatrixW06EwaCombinedDataCompAck)
{
    for (const ProfileKind profile : WriteProfiles)
        for (const DependencyPosition position : DependencyPositions)
        {
            const Row row{ profile, "W06" };
            Options options = ProfileOptions(profile);
            options.memoryLatency = 2;
            options.timing.fastResponse = 2;
            options.timing.normalResponse = 3;
            options.timing.output = 1;
            options.timing.commitSchedule = 2;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            impl.time = 60;
            Request request = WriteRequest(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
            request.expCompAck = true;
            request.memAttr |= CHI::Eb::MemAttrs::EWA;
            Transaction& transaction = AddBoundWrite(row, driver, request);
            const TransactionHandle handle = transaction.handle;
            impl.ResolveNonCoherent(transaction);
            impl.ActionNonCoherent(transaction);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 63);
            const Time accepted = PlaceDependency(driver, position, 63);
            SendWriteData(row, driver, transaction, WritePattern(61), FullWriteEnables(),
                CHI::Eb::Opcodes::DAT::NCBWrDataCompAck);
            TIMING_CHECK(row, transaction.Complete(COMPLETE_WRITE_DATA));
            TIMING_CHECK(row, transaction.Complete(COMPLETE_COMPACK));
            const Time action = CompleteInternalNonCoherent(row, driver, transaction, accepted);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, action + 4);
            TIMING_CHECK(row, transaction.phase == Phase::Commit);
            TIMING_CHECK(row, !impl.flows.at(handle).responseOwned);
            SendCompAck(driver, transaction);
            TIMING_CHECK(row, impl.errors.size() == 1);
            TIMING_CHECK(row, transaction.phase == Phase::Commit);
            impl.RetireFinished();
            driver.TickTo(action + 6);
            TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
            TIMING_CHECK(row, impl.ValidateInvariants());
        }
}

LUNATIC_TEST(Stage10MatrixW07CancellationJoinsQueuedResponses)
{
    for (const ProfileKind profile : WriteProfiles)
    {
        const Row row{ profile, "W07" };
        Options options = ProfileOptions(profile);
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.strategy.immediateResponse = ImmediateResponseKind::EarlyDBID;
        options.timing.fastResponse = 2;
        options.timing.normalResponse = 4;
        options.timing.output = 1;
        options.timing.commitSchedule = 1;
        const LineKey line = LineKey::FromAddress(0x100000, false);
        const LineData original = WritePattern(71);
        {
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            impl.memory.WriteFull(line, original);
            impl.time = 70;
            Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(CHI::Eb::Opcodes::REQ::WriteUniqueFull));
            const TransactionHandle handle = transaction.handle;
            impl.ResolveImmediateWrite(transaction);
            impl.QueueImmediateResponse(transaction);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 73);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, 75);
            SendCancel(row, driver, transaction);
            impl.StartBackendWrite(transaction);
            TIMING_CHECK(row, impl.txreq.empty());
            TIMING_CHECK(row, !impl.flows.at(handle).backendOperationOwned);
            impl.ActionImmediateWrite(transaction);
            TIMING_CHECK(row, transaction.phase == Phase::Commit);
            impl.RetireFinished();
            driver.TickTo(76);
            TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
            TIMING_CHECK(row, impl.memory.Read(line) == original);
            TIMING_CHECK(row, !impl.Bank(line).directory.Find(line));
            CheckClean(row, driver);
        }
        {
            Options retryOptions = options;
            retryOptions.timing.backendRequest = 2;
            retryOptions.timing.backendResend = 3;
            CoreDriver driver(retryOptions);
            Core::Impl& impl = driver.impl;
            impl.memory.WriteFull(line, original);
            impl.time = 70;
            Request request = WriteRequest(CHI::Eb::Opcodes::REQ::WriteUniqueFull);
            request.memAttr = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
            Transaction& transaction = AddBoundWrite(row, driver, request);
            const TransactionHandle handle = transaction.handle;
            impl.ResolveImmediateWrite(transaction);
            impl.QueueImmediateResponse(transaction);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 73);
            SendWriteData(row, driver, transaction, WritePattern(73), FullWriteEnables());
            impl.ActionImmediateWrite(transaction);
            TIMING_CHECK(row, transaction.phase == Phase::Action);
            TIMING_CHECK(row, impl.txreq.size() == 1);
            SendCancel(row, driver, transaction);
            PopBackendRequest(row, driver, 76, CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
            TIMING_CHECK(row, impl.HandleBackendRetry(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::RetryAck)));
            TIMING_CHECK(row, impl.flows.at(handle).backendOperationOwned);
            TIMING_CHECK(row, impl.backendRetries.contains(handle));
            impl.backendCredits[{ 6, 0 }] = 1;
            impl.AdvanceBackendRetries();
            TIMING_CHECK(row, impl.backendRetries.empty());
            TIMING_CHECK(row, !impl.flows.at(handle).backendOperationOwned);
            TIMING_CHECK(row, impl.txreq.size() == 1);
            TIMING_CHECK(row, impl.txreq.front().flit.opcode == CHI::Eb::Opcodes::REQ::PCrdReturn);
            ExpectTime(row, impl.txreq.front().readyTime, 80, "canceled backend credit return");
            PopBackendRequest(row, driver, 80, CHI::Eb::Opcodes::REQ::PCrdReturn);
            TIMING_CHECK(row, impl.txdat.empty());
            impl.ActionImmediateWrite(transaction);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, 80);
            impl.RetireFinished();
            driver.TickTo(81);
            TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
            TIMING_CHECK(row, impl.memory.Read(line) == original);
            CheckClean(row, driver);
        }
        {
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            impl.memory.WriteFull(line, original);
            impl.time = 70;
            Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(CHI::Eb::Opcodes::REQ::WriteUniqueFull));
            const TransactionHandle handle = transaction.handle;
            impl.ResolveImmediateWrite(transaction);
            impl.QueueImmediateResponse(transaction);
            TIMING_CHECK(row, impl.txrsp.size() == 2);
            SendCancel(row, driver, transaction);
            TIMING_CHECK(row, impl.txrsp.size() == 2);
            impl.StartBackendWrite(transaction);
            TIMING_CHECK(row, impl.txreq.empty());
            impl.ActionImmediateWrite(transaction);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 73);
            TIMING_CHECK(row, impl.tracker.Find(handle) != nullptr);
            TIMING_CHECK(row, impl.flows.at(handle).responseOwned);
            driver.TickTo(74);
            TIMING_CHECK(row, impl.tracker.Find(handle) != nullptr);
            TIMING_CHECK(row, impl.txrsp.size() == 1);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, 75);
            TIMING_CHECK(row, !impl.flows.at(handle).responseOwned);
            impl.RetireFinished();
            driver.TickTo(76);
            TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
            TIMING_CHECK(row, impl.memory.Read(line) == original);
            TIMING_CHECK(row, !impl.Bank(line).directory.Find(line));
            CheckClean(row, driver);
        }
    }
}

LUNATIC_TEST(Stage10MatrixW08WriteUniqueCleanHitNoSnoopSchedule)
{
    for (const ProfileKind profile : WriteProfiles)
        for (const DependencyPosition position : DependencyPositions)
        {
            const Row row{ profile, "W08" };
            Options options = ProfileOptions(profile);
            options.timing.snoopDecision = 4;
            options.timing.fastResponse = 2;
            options.timing.normalResponse = 5;
            options.timing.output = 1;
            options.timing.commitSchedule = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Transaction& dummy = driver.Add(WriteRequest(CHI::Eb::Opcodes::REQ::WriteUniqueFull, 99, 0x100100));
            impl.Retire(dummy.handle);
            impl.time = 80;
            Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(CHI::Eb::Opcodes::REQ::WriteUniqueFull));
            const TransactionHandle handle = transaction.handle;
            TIMING_CHECK(row, handle == 1);
            const LineKey line = transaction.request.Line();
            const LineData initial = WritePattern(83);
            const LineData update = WritePattern(89);
            SeedHome(driver, transaction, initial, false);
            TIMING_CHECK(row, impl.Bank(line).snoopFilter.Targets(line).empty());
            impl.ResolveImmediateWrite(transaction);
            TIMING_CHECK(row, !transaction.snoopsRequired);
            ExpectTime(row, transaction.readyTime, 84, "empty snoop decision");
            ExpectTime(row, impl.flows.at(handle).scheduleMarks.at(SchedulePoint::SnoopDecision), 84,
                "empty snoop decision mark");
            impl.time = 84;
            impl.QueueImmediateResponse(transaction);
            const uint16_t first = profile == ProfileKind::Type3
                ? CHI::Eb::Opcodes::RSP::Comp : CHI::Eb::Opcodes::RSP::DBIDResp;
            const uint16_t second = profile == ProfileKind::Type3
                ? CHI::Eb::Opcodes::RSP::DBIDResp : CHI::Eb::Opcodes::RSP::Comp;
            TIMING_CHECK(row, impl.txrsp.size() == 2);
            TIMING_CHECK(row, impl.txrsp[0].flit.opcode == first);
            TIMING_CHECK(row, impl.txrsp[1].flit.opcode == second);
            ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::DBIDResp, 87);
            ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::Comp, 90);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 87);
            const Time accepted = PlaceDependency(driver, position, 87);
            SendWriteData(row, driver, transaction, update, FullWriteEnables());
            impl.ActionImmediateWrite(transaction);
            TIMING_CHECK(row, impl.flows.at(handle).directoryCommitted);
            const Time compPopped = accepted > 90 ? accepted : 90;
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, compPopped);
            impl.RetireFinished();
            driver.TickTo(compPopped + 1);
            TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
            const HomeLine* home = impl.Bank(line).directory.Find(line);
            TIMING_CHECK(row, home && home->data == update && home->dirty);
            TIMING_CHECK(row, impl.txsnp.empty());
            CheckClean(row, driver);
        }
}

LUNATIC_TEST(Stage10MatrixW09PartialWriteBaselineDependencyPositions)
{
    for (const ProfileKind profile : WriteProfiles)
        for (const DependencyPosition position : DependencyPositions)
        {
            const Row row{ profile, "W09" };
            Options options = ProfileOptions(profile);
            options.backend = BackendKind::CHI;
            options.subordinateId = 6;
            options.timing.dataReturn = 2;
            options.timing.fastResponse = 2;
            options.timing.normalResponse = 4;
            options.timing.backendRequest = 2;
            options.timing.output = 1;
            options.timing.commitSchedule = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            impl.time = 90;
            Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(CHI::Eb::Opcodes::REQ::WriteUniquePtl));
            const TransactionHandle handle = transaction.handle;
            const LineKey line = transaction.request.Line();
            const LineData baseline = WritePattern(97);
            const LineData update = WritePattern(101);
            const ByteEnables enables = SparseWriteEnables();
            impl.memory.WriteFull(line, baseline);
            impl.ResolveImmediateWrite(transaction);
            impl.QueueImmediateResponse(transaction);
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 93);
            SendWriteData(row, driver, transaction, update, enables);
            TIMING_CHECK(row, !impl.PrepareHomeCommit(transaction));
            FlowState& flow = impl.flows.at(handle);
            TIMING_CHECK(row, flow.baselineFetch);
            TIMING_CHECK(row, flow.backendOperationOwned);
            TIMING_CHECK(row, flow.baselineOperationOwned);
            TIMING_CHECK(row, impl.txreq.size() == 1);
            TIMING_CHECK(row, impl.txreq.front().flit.opcode == CHI::Eb::Opcodes::REQ::ReadNoSnp);
            ExpectTime(row, impl.txreq.front().readyTime, 96, "baseline read request");
            PopBackendRequest(row, driver, 96, CHI::Eb::Opcodes::REQ::ReadNoSnp);
            const Time accepted = PlaceDependency(driver, position, 96);
            SendBackendData(row, driver, transaction, baseline);
            TIMING_CHECK(row, !flow.backendOperationOwned);
            TIMING_CHECK(row, flow.baselineOperationOwned);
            TIMING_CHECK(row, flow.baselineFetched);
            const Time dataDone = accepted + options.dataLatency + options.timing.dataReturn;
            ExpectTime(row, flow.scheduleMarks.at(SchedulePoint::DataReturn), dataDone, "baseline merge schedule");
            driver.TickTo(dataDone);
            TIMING_CHECK(row, flow.directoryCommitted);
            TIMING_CHECK(row, !flow.baselineOperationOwned);
            LineData expected = baseline;
            for (size_t index = 0; index < LINE_SIZE; ++index)
                if (enables[index])
                    expected[index] = update[index];
            const HomeLine* home = impl.Bank(line).directory.Find(line);
            TIMING_CHECK(row, home && home->data == expected);
            ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::Comp, 95);
            const Time compPopped = impl.time > 95 ? impl.time : 95;
            PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, compPopped);
            impl.RetireFinished();
            driver.TickTo(compPopped + 1);
            TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
            CheckClean(row, driver);
        }
}

LUNATIC_TEST(Stage10MatrixW10ZeroWritesRetainBackendCompletionGates)
{
    const std::array<uint16_t, 2> opcodes = {
        CHI::Eb::Opcodes::REQ::WriteNoSnpZero, CHI::Eb::Opcodes::REQ::WriteUniqueZero
    };
    for (const ProfileKind profile : WriteProfiles)
        for (const uint16_t opcode : opcodes)
            for (const DependencyPosition position : DependencyPositions)
            {
                const Row row{ profile, "W10" };
                Options options = ProfileOptions(profile);
                options.backend = BackendKind::CHI;
                options.subordinateId = 6;
                options.timing.fastResponse = 2;
                options.timing.normalResponse = 4;
                options.timing.backendRequest = 2;
                options.timing.output = 1;
                options.timing.commitSchedule = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                impl.time = 100;
                Request request = WriteRequest(opcode);
                if (opcode == CHI::Eb::Opcodes::REQ::WriteUniqueZero)
                    request.memAttr = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
                Transaction& transaction = AddBoundWrite(row, driver, request);
                const TransactionHandle handle = transaction.handle;
                const LineKey line = transaction.request.Line();
                if (opcode == CHI::Eb::Opcodes::REQ::WriteNoSnpZero)
                    impl.ResolveNonCoherent(transaction);
                else
                {
                    impl.ResolveImmediateWrite(transaction);
                    impl.ActionImmediateWrite(transaction);
                    ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::DBIDResp, 103);
                    ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::Comp, 105);
                    impl.ActionImmediateWrite(transaction);
                }
                TIMING_CHECK(row, transaction.phase != Phase::WaitWriteData);
                TIMING_CHECK(row, impl.flows.at(handle).backendPhase == BackendPhase::WaitRequestEmission);
                TIMING_CHECK(row, transaction.receivedBeats == 0);
                TIMING_CHECK(row, impl.rxdat.empty() && impl.txdat.empty());
                TIMING_CHECK(row, impl.txreq.size() == 1);
                ExpectTime(row, impl.txreq.front().readyTime, 103, "zero-write backend request");
                PopBackendRequest(row, driver, 103, CHI::Eb::Opcodes::REQ::WriteNoSnpZero);
                TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                    BackendResponse(transaction, CHI::Eb::Opcodes::RSP::DBIDResp)));
                TIMING_CHECK(row, impl.flows.at(handle).backendOperationOwned);
                TIMING_CHECK(row, impl.tracker.Find(handle) != nullptr);
                const Time accepted = PlaceDependency(driver, position, 103);
                TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                    BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
                TIMING_CHECK(row, !impl.flows.at(handle).backendOperationOwned);
                Time completed = accepted;
                if (opcode == CHI::Eb::Opcodes::REQ::WriteNoSnpZero)
                {
                    impl.ActionNonCoherent(transaction);
                    ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::CompDBIDResp, accepted + 3);
                    completed = accepted + 3;
                    PopResponse(row, driver, CHI::Eb::Opcodes::RSP::CompDBIDResp, completed);
                }
                else
                {
                    impl.ActionImmediateWrite(transaction);
                    TIMING_CHECK(row, impl.flows.at(handle).directoryCommitted);
                    PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, accepted);
                    completed = accepted > 105 ? accepted : 105;
                    PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, completed);
                    TIMING_CHECK(row, impl.memory.Read(line) == LineData{});
                }
                TIMING_CHECK(row, impl.txdat.empty());
                impl.RetireFinished();
                driver.TickTo(completed + 1);
                TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
                CheckClean(row, driver);
            }
}

LUNATIC_TEST(Stage10MatrixW11WriteUniquePeerAndSnoopMeDependencies)
{
    for (const bool snoopMe : { false, true })
        for (const ProfileKind profile : WriteProfiles)
            for (const bool dirtyPeer : { false, true })
                for (const DependencyPosition position : DependencyPositions)
                {
                    const Row row{ profile, "W11" };
                    Options options = ProfileOptions(profile);
                    options.timing.snoopDecision = 2;
                    options.timing.snoopIssue = 3;
                    options.timing.fastResponse = 2;
                    options.timing.normalResponse = 4;
                    options.timing.output = 1;
                    options.timing.commitSchedule = 1;
                    CoreDriver driver(options);
                    Core::Impl& impl = driver.impl;
                    impl.time = 110;
                    Request request = WriteRequest(CHI::Eb::Opcodes::REQ::WriteUniqueFull);
                    request.snoopMe = snoopMe;
                    Transaction& transaction = AddBoundWrite(row, driver, request);
                    const TransactionHandle handle = transaction.handle;
                    const LineKey line = transaction.request.Line();
                    const LineData peerData = WritePattern(107);
                    const LineData update = WritePattern(109);
                    impl.Bank(line).snoopFilter.Track(line, 5, dirtyPeer ? CacheState::UD : CacheState::UC, 0);
                    if (snoopMe)
                        impl.Bank(line).snoopFilter.Track(line, 4, CacheState::SC, 1);
                    impl.ResolveImmediateWrite(transaction);
                    TIMING_CHECK(row, transaction.snoopTargets.size() == (snoopMe ? 2 : 1));
                    bool peerPlanned = false;
                    bool sourcePlanned = false;
                    for (const NodeId target : transaction.snoopTargets)
                    {
                        peerPlanned |= target == 5;
                        sourcePlanned |= target == 4;
                    }
                    TIMING_CHECK(row, peerPlanned);
                    TIMING_CHECK(row, sourcePlanned == snoopMe);
                    ExpectTime(row, transaction.readyTime, 112, "write snoop decision");
                    impl.time = 112;
                    FlowState& flow = impl.flows.at(handle);
                    std::map<NodeId, TransactionId> snoopIds;
                    while (flow.nextSnoop < transaction.snoopTargets.size())
                    {
                        const Time issued = impl.time;
                        TIMING_CHECK(row, impl.EmitSnoops(transaction));
                        TIMING_CHECK(row, impl.txsnp.size() == 1);
                        const Pending<Snoop>& pending = impl.txsnp.front();
                        const NodeId target = pending.flit.target;
                        TIMING_CHECK(row, pending.flit.opcode == CHI::Eb::Opcodes::SNP::SnpUnique);
                        ExpectTime(row, pending.readyTime, issued + 4, "write snoop visibility");
                        snoopIds[target] = pending.flit.txnId;
                        impl.time = pending.readyTime;
                        PopSnoop(row, driver, impl.time, CHI::Eb::Opcodes::SNP::SnpUnique);
                    }
                    const Time accepted = PlaceDependency(driver, position, impl.time);
                    for (const NodeId target : transaction.snoopTargets)
                    {
                        const bool withData = target == 5 && dirtyPeer;
                        SendSnoopResponse(row, driver, transaction, target, snoopIds.at(target), withData, peerData);
                    }
                    TIMING_CHECK(row, transaction.pendingSnoops.empty());
                    TIMING_CHECK(row, !flow.snoopWaveOwned);
                    TIMING_CHECK(row, impl.Bank(line).activeSnoopWaves == 0);
                    impl.QueueImmediateResponse(transaction);
                    ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::DBIDResp, accepted + 3);
                    ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::Comp, accepted + 5);
                    PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, accepted + 3);
                    SendWriteData(row, driver, transaction, update, FullWriteEnables());
                    impl.ActionImmediateWrite(transaction);
                    TIMING_CHECK(row, flow.directoryCommitted);
                    PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, accepted + 5);
                    impl.RetireFinished();
                    driver.TickTo(accepted + 6);
                    TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
                    const HomeLine* home = impl.Bank(line).directory.Find(line);
                    TIMING_CHECK(row, home && home->data == update);
                    TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 5) == CacheState::I);
                    if (snoopMe)
                        TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) == CacheState::I);
                    CheckClean(row, driver);
                }
}

LUNATIC_TEST(Stage10MatrixW12ImmediateResponseStrategyTimingAndFallback)
{
    const std::array<ImmediateResponseKind, 4> strategies = {
        ImmediateResponseKind::Rotate, ImmediateResponseKind::EarlyDBID,
        ImmediateResponseKind::LateComp, ImmediateResponseKind::Combined
    };
    for (const ProfileKind profile : WriteProfiles)
        for (const ImmediateResponseKind strategy : strategies)
            for (size_t rotation = 0; rotation < (strategy == ImmediateResponseKind::Rotate ? 3 : 1); ++rotation)
                for (const DependencyPosition position : DependencyPositions)
                {
                    const Row row{ profile, "W12" };
                    Options options = ProfileOptions(profile);
                    options.strategy.immediateResponse = strategy;
                    options.timing.fastResponse = 2;
                    options.timing.normalResponse = 5;
                    options.timing.output = 1;
                    options.timing.commitSchedule = 1;
                    CoreDriver driver(options);
                    Core::Impl& impl = driver.impl;
                    for (size_t index = 0; index < rotation; ++index)
                    {
                        Transaction& dummy = driver.Add(WriteRequest(CHI::Eb::Opcodes::REQ::WriteUniqueFull,
                            90 + index, 0x100000 + (index + 1) * LINE_SIZE));
                        impl.Retire(dummy.handle);
                    }
                    impl.time = 120;
                    Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(CHI::Eb::Opcodes::REQ::WriteUniqueFull));
                    const TransactionHandle handle = transaction.handle;
                    TIMING_CHECK(row, handle == rotation);
                    impl.ResolveImmediateWrite(transaction);
                    impl.QueueImmediateResponse(transaction);
                    const size_t form = strategy == ImmediateResponseKind::Rotate ? rotation
                        : strategy == ImmediateResponseKind::EarlyDBID ? 0
                        : strategy == ImmediateResponseKind::LateComp ? 1 : 2;
                    std::vector<uint16_t> expected;
                    if (form == 0)
                        expected = { CHI::Eb::Opcodes::RSP::DBIDResp, CHI::Eb::Opcodes::RSP::Comp };
                    else if (form == 1)
                        expected = { CHI::Eb::Opcodes::RSP::Comp, CHI::Eb::Opcodes::RSP::DBIDResp };
                    else
                        expected = { CHI::Eb::Opcodes::RSP::CompDBIDResp };
                    TIMING_CHECK(row, impl.txrsp.size() == expected.size());
                    for (size_t index = 0; index < expected.size(); ++index)
                    {
                        TIMING_CHECK(row, impl.txrsp[index].flit.opcode == expected[index]);
                        const Time delay = expected[index] == CHI::Eb::Opcodes::RSP::Comp ? 6 : 3;
                        ExpectTime(row, impl.txrsp[index].readyTime, 120 + delay, "strategy response visibility");
                    }
                    Time completed = 0;
                    if (form == 2)
                    {
                        PopResponse(row, driver, CHI::Eb::Opcodes::RSP::CompDBIDResp, 123);
                        completed = PlaceDependency(driver, position, 123);
                        SendWriteData(row, driver, transaction, WritePattern(127), FullWriteEnables());
                        impl.ActionImmediateWrite(transaction);
                    }
                    else
                    {
                        PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 123);
                        const Time accepted = PlaceDependency(driver, position, 123);
                        SendWriteData(row, driver, transaction, WritePattern(127), FullWriteEnables());
                        impl.ActionImmediateWrite(transaction);
                        completed = accepted > 126 ? accepted : 126;
                        PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, completed);
                    }
                    TIMING_CHECK(row, transaction.phase == Phase::Commit);
                    impl.RetireFinished();
                    driver.TickTo(completed + 1);
                    TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
                    CheckClean(row, driver);
                }
    for (const ProfileKind profile : WriteProfiles)
    {
        const Row row{ profile, "W12" };
        Options options = ProfileOptions(profile);
        options.backend = BackendKind::AXI;
        options.strategy.immediateResponse = ImmediateResponseKind::Combined;
        options.timing.fastResponse = 2;
        options.timing.normalResponse = 5;
        options.timing.output = 1;
        options.timing.commitSchedule = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        impl.time = 120;
        Request request = WriteRequest(CHI::Eb::Opcodes::REQ::WriteUniqueFull);
        request.memAttr = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        Transaction& transaction = AddBoundWrite(row, driver, request);
        const TransactionHandle handle = transaction.handle;
        impl.ResolveImmediateWrite(transaction);
        impl.QueueImmediateResponse(transaction);
        TIMING_CHECK(row, impl.txrsp.size() == 1);
        ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::DBIDResp, 123);
        PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 123);
        SendWriteData(row, driver, transaction, WritePattern(131), FullWriteEnables());
        impl.ActionImmediateWrite(transaction);
        TIMING_CHECK(row, impl.axi.PeekAW().has_value());
        TIMING_CHECK(row, impl.txrsp.empty());
        driver.TickTo(124);
        const auto aw = impl.axi.PopAW();
        TIMING_CHECK(row, aw.has_value());
        driver.TickTo(125);
        TIMING_CHECK(row, impl.axi.PopW().has_value());
        TIMING_CHECK(row, impl.axi.PopW().has_value());
        driver.TickTo(126);
        AXIWriteResponse response;
        response.id = aw->id;
        TIMING_CHECK(row, impl.axi.PushB(response));
        impl.Tick();
        ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::Comp, 132);
        PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, 132);
        impl.RetireFinished();
        driver.TickTo(133);
        TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
        CheckClean(row, driver);
    }
    for (const ProfileKind profile : WriteProfiles)
    {
        const Row row{ profile, "W12" };
        Options options = ProfileOptions(profile);
        options.strategy.immediateResponse = ImmediateResponseKind::Combined;
        options.timing.fastResponse = 2;
        options.timing.normalResponse = 5;
        options.timing.output = 1;
        options.timing.commitSchedule = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        impl.time = 120;
        Request request = WriteRequest(CHI::Eb::Opcodes::REQ::WriteUniqueFull);
        request.order = 2;
        request.expCompAck = true;
        Transaction& transaction = AddBoundWrite(row, driver, request);
        const TransactionHandle handle = transaction.handle;
        impl.ResolveImmediateWrite(transaction);
        impl.QueueImmediateResponse(transaction);
        TIMING_CHECK(row, impl.txrsp.size() == 2);
        ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::DBIDRespOrd, 123);
        ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::Comp, 126);
        PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDRespOrd, 123);
        SendWriteData(row, driver, transaction, WritePattern(137), FullWriteEnables());
        impl.ActionImmediateWrite(transaction);
        PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, 126);
        TIMING_CHECK(row, transaction.phase == Phase::WaitCompAck);
        SendCompAck(driver, transaction);
        TIMING_CHECK(row, transaction.phase == Phase::Commit);
        impl.RetireFinished();
        driver.TickTo(127);
        TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
        CheckClean(row, driver);
    }
}

LUNATIC_TEST(Stage10MatrixW13DwtOverrideAndOrdinaryFallback)
{
    for (const ProfileKind profile : WriteProfiles)
        for (const bool enabled : { false, true })
            for (const DependencyPosition position : DependencyPositions)
            {
                if (!enabled && position != DependencyPosition::Early)
                    continue;
                const Row row{ profile, "W13" };
                Options options = ProfileOptions(profile);
                options.backend = BackendKind::CHI;
                options.subordinateId = 6;
                options.dwtEnabled = enabled;
                options.timing.fastResponse = 2;
                options.timing.normalResponse = 4;
                options.timing.backendRequest = 2;
                options.timing.backendData = 2;
                options.timing.dataBeat = 1;
                options.timing.output = 1;
                options.timing.commitSchedule = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                impl.time = 130;
                Request request = WriteRequest(CHI::Eb::Opcodes::REQ::WriteUniqueFull);
                request.memAttr = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
                Transaction& transaction = AddBoundWrite(row, driver, request);
                const TransactionHandle handle = transaction.handle;
                impl.ResolveImmediateWrite(transaction);
                FlowState& flow = impl.flows.at(handle);
                TIMING_CHECK(row, flow.dwtActive == enabled);
                if (!enabled)
                {
                    impl.ActionImmediateWrite(transaction);
                    ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::DBIDResp, 133);
                    PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 133);
                    SendWriteData(row, driver, transaction, WritePattern(137), FullWriteEnables());
                    impl.ActionImmediateWrite(transaction);
                    TIMING_CHECK(row, impl.txreq.size() == 1 && !impl.txreq.front().flit.doDwt);
                    const Request downstream = PopBackendRequest(row, driver, 136, CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
                    TIMING_CHECK(row, downstream.returnNid == 2);
                    TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                        BackendResponse(transaction, CHI::Eb::Opcodes::RSP::DBIDResp)));
                    impl.AdvanceBackendWrite(transaction);
                    impl.AdvanceBackendWrite(transaction);
                    driver.TickTo(139);
                    PopBackendData(row, driver, 139, 0);
                    PopBackendData(row, driver, 140, 2);
                    TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                        BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
                    impl.ActionImmediateWrite(transaction);
                    TIMING_CHECK(row, flow.directoryCommitted);
                    PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, 140);
                    impl.RetireFinished();
                    driver.TickTo(141);
                    TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
                    CheckClean(row, driver);
                    continue;
                }
                impl.ActionImmediateWrite(transaction);
                TIMING_CHECK(row, impl.txrsp.size() == 1);
                ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::Comp, 135);
                impl.ActionImmediateWrite(transaction);
                TIMING_CHECK(row, flow.directoryCommitted);
                TIMING_CHECK(row, impl.txreq.size() == 1);
                const Pending<Request>& pending = impl.txreq.front();
                TIMING_CHECK(row, pending.flit.doDwt);
                TIMING_CHECK(row, pending.flit.returnNid == 4 && pending.flit.returnTxnId == 1);
                const Request downstream = PopBackendRequest(row, driver, 133, CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
                TIMING_CHECK(row, downstream.doDwt && downstream.returnNid == 4);
                TIMING_CHECK(row, flow.backendPhase == BackendPhase::WaitComp);
                PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, 135);
                TIMING_CHECK(row, impl.tracker.Find(handle) != nullptr);
                TIMING_CHECK(row, flow.responseOwned && flow.backendOperationOwned);
                const Time accepted = PlaceDependency(driver, position, 135);
                TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                    BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
                TIMING_CHECK(row, !flow.backendOperationOwned);
                impl.ActionImmediateWrite(transaction);
                TIMING_CHECK(row, transaction.phase == Phase::Commit);
                TIMING_CHECK(row, !flow.responseOwned);
                TIMING_CHECK(row, impl.txdat.empty());
                impl.RetireFinished();
                driver.TickTo(accepted + 1);
                TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
                CheckClean(row, driver);
            }
}

LUNATIC_TEST(Stage10MatrixW14WriteUniqueStashTargetAndDataFallback)
{
    for (const ProfileKind profile : WriteProfiles)
        for (const bool full : { false, true })
            for (const bool present : { false, true })
                for (const DependencyPosition position : DependencyPositions)
                {
                    const Row row{ profile, "W14" };
                    Options options = ProfileOptions(profile);
                    options.timing.snoopDecision = 2;
                    options.timing.snoopIssue = 2;
                    options.timing.fastResponse = 2;
                    options.timing.normalResponse = 4;
                    options.timing.output = 1;
                    options.timing.commitSchedule = 1;
                    CoreDriver driver(options);
                    Core::Impl& impl = driver.impl;
                    impl.time = 140;
                    const uint16_t opcode = full ? CHI::Eb::Opcodes::REQ::WriteUniqueFullStash
                        : CHI::Eb::Opcodes::REQ::WriteUniquePtlStash;
                    Request request = WriteRequest(opcode);
                    request.returnNid = 5;
                    request.stashNidValid = true;
                    request.stashLpidValid = true;
                    request.stashLpid = 3;
                    Transaction& transaction = AddBoundWrite(row, driver, request);
                    const TransactionHandle handle = transaction.handle;
                    const LineKey line = transaction.request.Line();
                    const LineData initial = WritePattern(139);
                    const LineData peerData = WritePattern(149);
                    const LineData update = WritePattern(151);
                    const ByteEnables enables = full ? FullWriteEnables() : SparseWriteEnables();
                    SeedHome(driver, transaction, initial, false);
                    if (present)
                        impl.Bank(line).snoopFilter.Track(line, 5, CacheState::UD, 0);
                    impl.ResolveImmediateWrite(transaction);
                    TIMING_CHECK(row, transaction.snoopTargets.size() == 1);
                    TIMING_CHECK(row, transaction.snoopTargets.front() == 5);
                    TIMING_CHECK(row, !impl.flows.at(handle).dwtActive);
                    ExpectTime(row, transaction.readyTime, 142, "stash snoop decision");
                    impl.time = 142;
                    TIMING_CHECK(row, impl.EmitSnoops(transaction));
                    const uint16_t snoopOpcode = full ? CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash
                        : CHI::Eb::Opcodes::SNP::SnpUniqueStash;
                    TIMING_CHECK(row, impl.txsnp.front().flit.stashLpidValid);
                    TIMING_CHECK(row, impl.txsnp.front().flit.stashLpid == 3);
                    ExpectTime(row, impl.txsnp.front().readyTime, 145, "stash snoop visibility");
                    const Snoop snoop = PopSnoop(row, driver, 145, snoopOpcode);
                    const Time accepted = PlaceDependency(driver, position, 145);
                    SendSnoopResponse(row, driver, transaction, 5, snoop.txnId, !full && present, peerData);
                    TIMING_CHECK(row, transaction.pendingSnoops.empty());
                    TIMING_CHECK(row, !impl.flows.at(handle).snoopWaveOwned);
                    impl.QueueImmediateResponse(transaction);
                    PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, accepted + 3);
                    SendWriteData(row, driver, transaction, update, enables);
                    impl.ActionImmediateWrite(transaction);
                    TIMING_CHECK(row, impl.flows.at(handle).directoryCommitted);
                    PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, accepted + 5);
                    impl.RetireFinished();
                    driver.TickTo(accepted + 6);
                    TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
                    LineData expected = !full && present ? peerData : initial;
                    for (size_t index = 0; index < LINE_SIZE; ++index)
                        if (enables[index])
                            expected[index] = update[index];
                    const HomeLine* home = impl.Bank(line).directory.Find(line);
                    TIMING_CHECK(row, home && home->data == expected);
                    TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 5) == CacheState::I);
                    TIMING_CHECK(row, impl.txreq.empty() && !impl.flows.contains(handle));
                    CheckClean(row, driver);
                }
}

LUNATIC_TEST(Stage10MatrixW15CopyBackResponseAndCommitLifetime)
{
    const std::array<uint16_t, 2> opcodes = {
        CHI::Eb::Opcodes::REQ::WriteBackFull, CHI::Eb::Opcodes::REQ::WriteCleanFull
    };
    for (const ProfileKind profile : WriteProfiles)
        for (const uint16_t opcode : opcodes)
            for (const bool homeHit : { false, true })
                for (const DependencyPosition position : DependencyPositions)
                {
                    const Row row{ profile, "W15" };
                    Options options = ProfileOptions(profile);
                    options.timing.fastResponse = 3;
                    options.timing.output = 2;
                    options.timing.commitSchedule = 2;
                    CoreDriver driver(options);
                    Core::Impl& impl = driver.impl;
                    impl.time = 150;
                    Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(opcode));
                    const TransactionHandle handle = transaction.handle;
                    const LineKey line = transaction.request.Line();
                    const LineData initial = WritePattern(157);
                    const LineData update = WritePattern(163);
                    if (homeHit)
                        SeedHome(driver, transaction, initial, false);
                    impl.Bank(line).snoopFilter.Track(line, 4,
                        opcode == CHI::Eb::Opcodes::REQ::WriteBackFull ? CacheState::UD : CacheState::UC, 0);
                    impl.ResolveCoherent(transaction);
                    impl.ActionCoherent(transaction);
                    ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::CompDBIDResp, 155);
                    PopResponse(row, driver, CHI::Eb::Opcodes::RSP::CompDBIDResp, 155);
                    TIMING_CHECK(row, transaction.phase == Phase::WaitWriteData);
                    TIMING_CHECK(row, impl.flows.at(handle).responseOwned);
                    const Time accepted = PlaceDependency(driver, position, 155);
                    const uint8_t resp = opcode == CHI::Eb::Opcodes::REQ::WriteBackFull
                        ? CHI::Eb::Resps::CopyBackWrData_UD_PD : CHI::Eb::Resps::CopyBackWrData_UC;
                    SendWriteData(row, driver, transaction, update, FullWriteEnables(),
                        CHI::Eb::Opcodes::DAT::CopyBackWrData, resp);
                    TIMING_CHECK(row, impl.flows.at(handle).responseOwned);
                    impl.ActionCoherent(transaction);
                    TIMING_CHECK(row, transaction.phase == Phase::Commit);
                    TIMING_CHECK(row, impl.flows.at(handle).responseOwned);
                    impl.RetireFinished();
                    driver.TickTo(accepted + 1);
                    TIMING_CHECK(row, impl.tracker.Find(handle) != nullptr);
                    TIMING_CHECK(row, impl.flows.at(handle).responseOwned);
                    driver.TickTo(accepted + 2);
                    TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
                    TIMING_CHECK(row, impl.Bank(line).responseUnits == 0);
                    if (opcode == CHI::Eb::Opcodes::REQ::WriteBackFull)
                    {
                        const HomeLine* home = impl.Bank(line).directory.Find(line);
                        TIMING_CHECK(row, home && home->data == update && home->dirty);
                        TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) == CacheState::I);
                    }
                    else
                    {
                        TIMING_CHECK(row, !impl.Bank(line).directory.Find(line));
                        TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) == CacheState::UC);
                        TIMING_CHECK(row, impl.memory.Read(line) == update);
                    }
                    CheckClean(row, driver);
                }
}

LUNATIC_TEST(Stage10MatrixW16WriteEvictFullLocalPolicyConsumption)
{
    for (const ProfileKind profile : WriteProfiles)
        for (const bool homeDirty : { false, true })
            for (const bool requesterDirty : { false, true })
                for (const DependencyPosition position : DependencyPositions)
                {
                    const Row row{ profile, "W16" };
                    Options options = ProfileOptions(profile);
                    options.backend = BackendKind::CHI;
                    options.subordinateId = 6;
                    options.timing.fastResponse = 2;
                    options.timing.output = 1;
                    options.timing.commitSchedule = 1;
                    CoreDriver driver(options);
                    Core::Impl& impl = driver.impl;
                    impl.time = 160;
                    Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(CHI::Eb::Opcodes::REQ::WriteEvictFull));
                    const TransactionHandle handle = transaction.handle;
                    const LineKey line = transaction.request.Line();
                    const LineData initial = WritePattern(167);
                    const LineData update = WritePattern(173);
                    impl.memory.WriteFull(line, initial);
                    SeedHome(driver, transaction, initial, homeDirty);
                    impl.Bank(line).snoopFilter.Track(line, 4, requesterDirty ? CacheState::UD : CacheState::UC, 0);
                    impl.ResolveCoherent(transaction);
                    impl.ActionCoherent(transaction);
                    PopResponse(row, driver, CHI::Eb::Opcodes::RSP::CompDBIDResp, 163);
                    const Time accepted = PlaceDependency(driver, position, 163);
                    SendWriteData(row, driver, transaction, update, FullWriteEnables(),
                        CHI::Eb::Opcodes::DAT::CopyBackWrData,
                        requesterDirty ? CHI::Eb::Resps::CopyBackWrData_UD_PD : CHI::Eb::Resps::CopyBackWrData_UC);
                    impl.ActionCoherent(transaction);
                    TIMING_CHECK(row, transaction.phase == Phase::Commit);
                    TIMING_CHECK(row, impl.txreq.empty());
                    TIMING_CHECK(row, !impl.flows.at(handle).backendOperationOwned);
                    TIMING_CHECK(row, !impl.flows.at(handle).victimPending);
                    const HomeLine* home = impl.Bank(line).directory.Find(line);
                    TIMING_CHECK(row, home && home->data == update);
                    TIMING_CHECK(row, home->dirty == requesterDirty);
                    TIMING_CHECK(row, impl.memory.Read(line) == initial);
                    TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) == CacheState::I);
                    impl.RetireFinished();
                    driver.TickTo(accepted + 1);
                    TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
                    CheckClean(row, driver);
                }
}

LUNATIC_TEST(Stage10MatrixW17WriteEvictOrEvictDataAndNoDataForms)
{
    for (const ProfileKind profile : WriteProfiles)
        for (const bool withData : { false, true })
            for (const DependencyPosition position : DependencyPositions)
            {
                const Row row{ profile, "W17" };
                Options options = ProfileOptions(profile);
                options.timing.fastResponse = 2;
                options.timing.normalResponse = 4;
                options.timing.output = 1;
                options.timing.commitSchedule = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                impl.time = 170;
                Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(CHI::Eb::Opcodes::REQ::WriteEvictOrEvict));
                const TransactionHandle handle = transaction.handle;
                const LineKey line = transaction.request.Line();
                const LineData initial = WritePattern(179);
                const LineData update = WritePattern(181);
                if (withData)
                    impl.Bank(line).snoopFilter.Track(line, 4, CacheState::UD, 0);
                else
                {
                    SeedHome(driver, transaction, initial, false);
                    impl.Bank(line).snoopFilter.Track(line, 4, CacheState::SC, 0);
                }
                FlowState& flow = impl.flows.at(handle);
                flow.requesterWasValid = true;
                flow.requesterWasDirty = withData;
                impl.ResolveCoherent(transaction);
                impl.ActionEvictOrEvict(transaction);
                TIMING_CHECK(row, flow.evictOrEvictNoData == !withData);
                Time completed = 0;
                if (withData)
                {
                    TIMING_CHECK(row, flow.dbidSent);
                    ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::CompDBIDResp, 173);
                    PopResponse(row, driver, CHI::Eb::Opcodes::RSP::CompDBIDResp, 173);
                    completed = PlaceDependency(driver, position, 173);
                    SendWriteData(row, driver, transaction, update, FullWriteEnables(),
                        CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
                    impl.ActionEvictOrEvict(transaction);
                    TIMING_CHECK(row, transaction.phase == Phase::Commit);
                    TIMING_CHECK(row, !RequiresCompAck(transaction.request));
                    const HomeLine* home = impl.Bank(line).directory.Find(line);
                    TIMING_CHECK(row, home && home->data == update);
                }
                else
                {
                    TIMING_CHECK(row, !flow.dbidSent);
                    TIMING_CHECK(row, transaction.receivedBeats == 0);
                    ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::Comp, 175);
                    PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, 175);
                    TIMING_CHECK(row, transaction.phase == Phase::WaitCompAck);
                    TIMING_CHECK(row, flow.responseOwned);
                    completed = PlaceDependency(driver, position, 175);
                    SendCompAck(driver, transaction);
                    TIMING_CHECK(row, transaction.phase == Phase::Commit);
                    TIMING_CHECK(row, !flow.responseOwned);
                    const HomeLine* home = impl.Bank(line).directory.Find(line);
                    TIMING_CHECK(row, home && home->data == initial);
                }
                TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) == CacheState::I);
                TIMING_CHECK(row, impl.rxdat.empty());
                impl.RetireFinished();
                driver.TickTo(completed + 1);
                TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
                CheckClean(row, driver);
            }
}

namespace {

    void RunDirtyVictimSpill(const Row& row, ProfileKind profile, BackendKind backend,
                             DependencyPosition position)
    {
        Options options = ProfileOptions(profile);
        options.backend = backend;
        options.subordinateId = 6;
        options.policy = PolicyKind::Inclusive;
        options.slcSets = 1;
        options.slcWays = 1;
        options.timing.snoopIssue = 2;
        options.timing.fastResponse = 2;
        options.timing.backendRequest = 2;
        options.timing.backendData = 2;
        options.timing.dataBeat = 1;
        options.timing.output = 1;
        options.timing.commitSchedule = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        impl.time = 180;
        const LineKey victim = LineKey::FromAddress(0x180000, false);
        const LineKey line = LineKey::FromAddress(victim.address + options.structure.bankCount * LINE_SIZE, false);
        const LineData victimInitial = WritePattern(191);
        const LineData victimUpdate = WritePattern(193);
        const LineData update = WritePattern(197);
        impl.InstallHome(victim, 0, victimInitial, true, 0);
        impl.Bank(victim).snoopFilter.Track(victim, 5, CacheState::UD, 0);
        Transaction& transaction = AddBoundWrite(row, driver,
            WriteRequest(CHI::Eb::Opcodes::REQ::WriteBackFull, 1, line.address));
        const TransactionHandle handle = transaction.handle;
        impl.ResolveCoherent(transaction);
        impl.ActionCoherent(transaction);
        PopResponse(row, driver, CHI::Eb::Opcodes::RSP::CompDBIDResp, 183);
        SendWriteData(row, driver, transaction, update, FullWriteEnables(),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        TIMING_CHECK(row, !impl.PrepareHomeCommit(transaction));
        FlowState& flow = impl.flows.at(handle);
        TIMING_CHECK(row, flow.victimPending && flow.victimLinked);
        TIMING_CHECK(row, flow.victimLine == victim);
        TIMING_CHECK(row, transaction.snoopTargets.size() == 1 && transaction.snoopTargets.front() == 5);
        TIMING_CHECK(row, impl.EmitSnoops(transaction));
        ExpectTime(row, impl.txsnp.front().readyTime, 186, "victim snoop visibility");
        const Snoop snoop = PopSnoop(row, driver, 186, CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
        const Time snoopAccepted = PlaceDependency(driver, position, 186);
        SendSnoopResponse(row, driver, transaction, 5, snoop.txnId, true, victimUpdate);
        TIMING_CHECK(row, transaction.pendingSnoops.empty());
        TIMING_CHECK(row, flow.victimDirty && flow.victimData == victimUpdate);
        TIMING_CHECK(row, !impl.CompleteVictim(transaction));
        TIMING_CHECK(row, flow.spillPending && flow.victimPending);
        TIMING_CHECK(row, flow.backendOperationOwned);
        TIMING_CHECK(row, flow.spillLine == victim && flow.spillData == victimUpdate);
        Time completed = snoopAccepted;
        if (backend == BackendKind::CHI)
        {
            TIMING_CHECK(row, impl.txreq.size() == 1);
            TIMING_CHECK(row, impl.txreq.front().flit.opcode == CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
            const Time requestVisible = snoopAccepted + 3;
            ExpectTime(row, impl.txreq.front().readyTime, requestVisible, "dirty spill request");
            PopBackendRequest(row, driver, requestVisible, CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
            Time dataRequestVisible = requestVisible;
            if (position == DependencyPosition::Late)
            {
                TIMING_CHECK(row, impl.HandleBackendRetry(transaction,
                    BackendResponse(transaction, CHI::Eb::Opcodes::RSP::RetryAck)));
                TIMING_CHECK(row, flow.backendOperationOwned);
                impl.backendCredits[{ 6, 0 }] = 1;
                impl.AdvanceBackendRetries();
                TIMING_CHECK(row, impl.txreq.size() == 1);
                TIMING_CHECK(row, impl.txreq.front().schedule == SchedulePoint::BackendResend);
                dataRequestVisible = requestVisible + 1;
                ExpectTime(row, impl.txreq.front().readyTime, dataRequestVisible, "dirty spill resend");
                PopBackendRequest(row, driver, dataRequestVisible, CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
            }
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::DBIDResp)));
            if (position == DependencyPosition::Early)
                TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                    BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
            impl.AdvanceBackendWrite(transaction);
            const Time firstVisible = dataRequestVisible + 3;
            ExpectTime(row, impl.txdat.front().readyTime, firstVisible, "dirty spill first beat");
            impl.AdvanceBackendWrite(transaction);
            const Time secondVisible = dataRequestVisible + 4;
            driver.TickTo(firstVisible);
            TIMING_CHECK(row, impl.txdat.size() == 2);
            ExpectTime(row, impl.txdat[1].readyTime, secondVisible, "dirty spill second beat");
            PopBackendData(row, driver, firstVisible, 0);
            if (position == DependencyPosition::Same)
            {
                driver.TickTo(secondVisible);
                TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                    BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
            }
            TIMING_CHECK(row, flow.backendOperationOwned);
            PopBackendData(row, driver, secondVisible, 2);
            completed = secondVisible;
            if (position == DependencyPosition::Late)
            {
                TIMING_CHECK(row, flow.backendOperationOwned);
                completed = secondVisible + 4;
                driver.TickTo(completed);
                TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                    BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
            }
            TIMING_CHECK(row, !flow.backendOperationOwned);
        }
        else
        {
            TIMING_CHECK(row, impl.txreq.empty() && impl.txdat.empty());
            TIMING_CHECK(row, impl.axi.PeekAW().has_value());
            driver.TickTo(snoopAccepted + 1);
            const auto aw = impl.axi.PopAW();
            TIMING_CHECK(row, aw.has_value() && aw->len == 1);
            driver.TickTo(snoopAccepted + 2);
            TIMING_CHECK(row, impl.axi.PopW().has_value());
            const auto last = impl.axi.PopW();
            TIMING_CHECK(row, last.has_value() && last->last);
            completed = PlaceDependency(driver, position, snoopAccepted + 2);
            TIMING_CHECK(row, flow.backendOperationOwned);
            AXIWriteResponse response;
            response.id = aw->id;
            TIMING_CHECK(row, impl.axi.PushB(response));
            impl.Tick();
            TIMING_CHECK(row, !flow.backendOperationOwned);
        }
        if (transaction.phase != Phase::Commit)
            impl.ActionCoherent(transaction);
        TIMING_CHECK(row, transaction.phase == Phase::Commit);
        TIMING_CHECK(row, !flow.spillPending && !flow.victimPending && !flow.victimLinked);
        TIMING_CHECK(row, !flow.backendOperationOwned && !flow.snoopWaveOwned);
        TIMING_CHECK(row, !impl.Bank(victim).directory.Find(victim));
        TIMING_CHECK(row, impl.memory.Read(victim) == victimUpdate);
        const HomeLine* home = impl.Bank(line).directory.Find(line);
        TIMING_CHECK(row, home && home->data == update);
        TIMING_CHECK(row, impl.Bank(victim).snoopFilter.State(victim, 5) == CacheState::I);
        impl.RetireFinished();
        driver.TickTo(completed + 1);
        TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
        TIMING_CHECK(row, backend != BackendKind::AXI || !impl.axi.HasInflight());
        CheckClean(row, driver);
    }

}

LUNATIC_TEST(Stage10MatrixW18DirtyVictimChiAxiSpillTiming)
{
    for (const ProfileKind profile : WriteProfiles)
        for (const BackendKind backend : { BackendKind::CHI, BackendKind::AXI })
            for (const DependencyPosition position : DependencyPositions)
            {
                const Row row{ profile, "W18" };
                RunDirtyVictimSpill(row, profile, backend, position);
            }
}

LUNATIC_TEST(Stage10MatrixW19HeldResponsesWithCompletedRequesterData)
{
    for (const ProfileKind profile : WriteProfiles)
        for (size_t form = 0; form < 3; ++form)
        {
            const Row row{ profile, "W19" };
            Options options = ProfileOptions(profile);
            options.memoryLatency = 1;
            options.strategy.immediateResponse = form == 0 ? ImmediateResponseKind::EarlyDBID
                : form == 1 ? ImmediateResponseKind::LateComp : ImmediateResponseKind::Combined;
            options.timing.fastResponse = 2;
            options.timing.normalResponse = 4;
            options.timing.output = 1;
            options.timing.commitSchedule = 2;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            impl.time = 190;
            const uint16_t opcode = form == 0 ? CHI::Eb::Opcodes::REQ::WriteNoSnpFull
                : CHI::Eb::Opcodes::REQ::WriteUniqueFull;
            Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(opcode));
            const TransactionHandle handle = transaction.handle;
            if (form == 0)
            {
                impl.ResolveNonCoherent(transaction);
                impl.ActionNonCoherent(transaction);
            }
            else
            {
                impl.ResolveImmediateWrite(transaction);
                impl.QueueImmediateResponse(transaction);
            }
            const uint16_t heldOpcode = form == 0 ? CHI::Eb::Opcodes::RSP::DBIDResp
                : form == 1 ? CHI::Eb::Opcodes::RSP::Comp : CHI::Eb::Opcodes::RSP::CompDBIDResp;
            const Time visible = form == 1 ? 195 : 193;
            ExpectResponseTime(row, impl, heldOpcode, visible);
            if (form == 1)
                PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 193);
            driver.TickTo(visible);
            SendWriteData(row, driver, transaction, WritePattern(199), FullWriteEnables());
            TIMING_CHECK(row, transaction.Complete(COMPLETE_WRITE_DATA));
            if (form == 0)
                impl.ActionNonCoherent(transaction);
            else
                impl.ActionImmediateWrite(transaction);
            for (Time held = visible; held <= visible + 3; ++held)
            {
                driver.TickTo(held);
                TIMING_CHECK(row, impl.tracker.Find(handle) != nullptr);
                TIMING_CHECK(row, FindResponse(impl, heldOpcode).readyTime == visible);
                const uint64_t serial = impl.nextSerial;
                const Response* peeked = impl.Peek(impl.txrsp);
                TIMING_CHECK(row, peeked && peeked->opcode == heldOpcode);
                TIMING_CHECK(row, impl.nextSerial == serial);
            }
            PopResponse(row, driver, heldOpcode, visible + 3);
            Time completed = visible + 3;
            if (form == 0)
            {
                ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::Comp, 201);
                completed = 201;
                PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, completed);
            }
            TIMING_CHECK(row, transaction.phase == Phase::Commit);
            impl.RetireFinished();
            driver.TickTo(completed + 1);
            TIMING_CHECK(row, impl.tracker.Find(handle) != nullptr);
            driver.TickTo(completed + 2);
            TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
            TIMING_CHECK(row, impl.txrsp.empty());
            CheckClean(row, driver);
        }
}

namespace {

    void RunAxiWriteResponsePosition(const Row& row, ProfileKind profile, DependencyPosition position)
    {
        Options options = ProfileOptions(profile);
        options.backend = BackendKind::AXI;
        options.timing.fastResponse = 2;
        options.timing.normalResponse = 3;
        options.timing.output = 1;
        options.timing.commitSchedule = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        impl.time = 210;
        Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(CHI::Eb::Opcodes::REQ::WriteNoSnpZero));
        const TransactionHandle handle = transaction.handle;
        impl.ResolveNonCoherent(transaction);
        driver.TickTo(211);
        const auto aw = impl.axi.PopAW();
        TIMING_CHECK(row, aw.has_value());
        driver.TickTo(212);
        TIMING_CHECK(row, impl.axi.PopW().has_value());
        const auto last = impl.axi.PopW();
        TIMING_CHECK(row, last.has_value() && last->last);
        TIMING_CHECK(row, impl.txrsp.empty());
        const Time accepted = PlaceDependency(driver, position, 212);
        AXIWriteResponse response;
        response.id = aw->id;
        TIMING_CHECK(row, impl.axi.PushB(response));
        impl.Tick();
        ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::CompDBIDResp, accepted + 3);
        PopResponse(row, driver, CHI::Eb::Opcodes::RSP::CompDBIDResp, accepted + 3);
        impl.RetireFinished();
        driver.TickTo(accepted + 4);
        TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
        TIMING_CHECK(row, !impl.axi.HasInflight());
        CheckClean(row, driver);
    }

    void RunChiWriteResponseOrder(const Row& row, ProfileKind profile, DependencyPosition position,
                                  size_t order)
    {
        Options options = ProfileOptions(profile);
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.timing.fastResponse = 2;
        options.timing.normalResponse = 3;
        options.timing.backendRequest = 1;
        options.timing.backendData = 1;
        options.timing.dataBeat = 1;
        options.timing.output = 1;
        options.timing.commitSchedule = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        impl.time = 200;
        Transaction& transaction = AddBoundWrite(row, driver, WriteRequest(CHI::Eb::Opcodes::REQ::WriteNoSnpFull));
        const TransactionHandle handle = transaction.handle;
        impl.ResolveNonCoherent(transaction);
        impl.ActionNonCoherent(transaction);
        PopResponse(row, driver, CHI::Eb::Opcodes::RSP::DBIDResp, 203);
        SendWriteData(row, driver, transaction, WritePattern(211), FullWriteEnables());
        impl.ActionNonCoherent(transaction);
        PopBackendRequest(row, driver, 205, CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
        FlowState& flow = impl.flows.at(handle);
        TIMING_CHECK(row, !impl.HandleBackendRSP(transaction,
            BackendResponse(transaction, CHI::Eb::Opcodes::RSP::CompPersist)));
        TIMING_CHECK(row, flow.backendOperationOwned);
        TIMING_CHECK(row, impl.txrsp.empty());
        const Time accepted = PlaceDependency(driver, position, 205);
        if (order == 0)
        {
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::DBIDResp)));
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
        }
        else if (order == 1)
        {
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::Comp)));
            TIMING_CHECK(row, flow.backendCompReceived);
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::DBIDResp)));
        }
        else
            TIMING_CHECK(row, impl.HandleBackendRSP(transaction,
                BackendResponse(transaction, CHI::Eb::Opcodes::RSP::CompDBIDResp)));
        impl.AdvanceBackendWrite(transaction);
        const Time firstVisible = accepted + 2;
        ExpectTime(row, impl.txdat.front().readyTime, firstVisible, "response-order first data beat");
        impl.AdvanceBackendWrite(transaction);
        const Time secondVisible = accepted + 3;
        driver.TickTo(firstVisible);
        TIMING_CHECK(row, impl.txdat.size() == 2);
        ExpectTime(row, impl.txdat[1].readyTime, secondVisible, "response-order second data beat");
        PopBackendData(row, driver, firstVisible, 0);
        PopBackendData(row, driver, secondVisible, 2);
        TIMING_CHECK(row, !flow.backendOperationOwned);
        TIMING_CHECK(row, flow.backendCompReceived);
        impl.ActionNonCoherent(transaction);
        ExpectResponseTime(row, impl, CHI::Eb::Opcodes::RSP::Comp, secondVisible + 4);
        PopResponse(row, driver, CHI::Eb::Opcodes::RSP::Comp, secondVisible + 4);
        impl.RetireFinished();
        driver.TickTo(secondVisible + 5);
        TIMING_CHECK(row, impl.tracker.Find(handle) == nullptr);
        CheckClean(row, driver);
    }

}

LUNATIC_TEST(Stage10MatrixW20BackendResponseOrdersAndUnsupportedForms)
{
    for (const ProfileKind profile : WriteProfiles)
        for (const DependencyPosition position : DependencyPositions)
        {
            const Row row{ profile, "W20" };
            RunAxiWriteResponsePosition(row, profile, position);
        }
    for (const ProfileKind profile : WriteProfiles)
        for (const size_t order : { size_t(0), size_t(2) })
            for (const DependencyPosition position : DependencyPositions)
            {
                const Row row{ profile, "W20" };
                RunChiWriteResponseOrder(row, profile, position, order);
            }
    for (const ProfileKind profile : WriteProfiles)
        for (const DependencyPosition position : DependencyPositions)
        {
            const Row row{ profile, "W20" };
            RunChiWriteResponseOrder(row, profile, position, 1);
        }
}
