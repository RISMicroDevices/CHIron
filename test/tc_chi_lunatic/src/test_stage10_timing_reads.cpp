#include "timing_matrix.hpp"

#include <array>
#include <limits>
#include <vector>

using namespace CHI::Lunatic;
using namespace LunaticTest;
using namespace LunaticTest::Timing;

namespace {

    const std::array<ProfileKind, 2> Profiles = { ProfileKind::Type3, ProfileKind::Type7 };

    LineData ReadPattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 13);
        return data;
    }

    void SeedHome(Core::Impl& impl, const LineKey& line, const LineData& data, bool dirty = false)
    {
        HomeLine& home = impl.InstallHome(line, 0, data, dirty, 0);
        home.validBytes.fill(1);
    }

    Response MakeResponse(NodeId source, TransactionId txnId, uint16_t opcode,
                          uint8_t resp = 0, uint8_t respErr = 0)
    {
        Response response;
        response.source = source;
        response.target = 2;
        response.txnId = txnId;
        response.opcode = opcode;
        response.resp = resp;
        response.respErr = respErr;
        return response;
    }

    Data MakeData(NodeId source, NodeId target, TransactionId txnId, size_t beat,
                  const Options& options, uint16_t opcode, const LineData& payload,
                  uint8_t resp = 0, uint8_t respErr = 0, bool partial = false)
    {
        const size_t bytes = BytesPerBeat(options);
        const size_t offset = beat * bytes;
        Data data;
        data.source = source;
        data.target = target;
        data.homeNid = 2;
        data.txnId = txnId;
        data.opcode = opcode;
        data.resp = resp;
        data.respErr = respErr;
        data.dataId = offset / 16;
        data.ccid = (offset / 16) % 2;
        for (size_t index = 0; index < bytes; ++index)
        {
            data.byteEnables[index] = !partial || index < bytes / 2;
            data.bytes[index] = data.byteEnables[index] ? payload[offset + index] : 0;
        }
        return data;
    }

    Data BackendData(const Transaction& transaction, size_t beat, const Options& options,
                     const LineData& payload, uint16_t opcode = CHI::Eb::Opcodes::DAT::CompData,
                     uint8_t respErr = 0)
    {
        Data data = MakeData(6, 2, transaction.handle, beat, options, opcode, payload,
            opcode == CHI::Eb::Opcodes::DAT::DataSepResp
                ? uint8_t(CHI::Eb::Resps::DataSepResp_UC) : uint8_t(CHI::Eb::Resps::CompData_I),
            respErr);
        data.ccid = (transaction.request.address >> 4) & 3;
        return data;
    }

    void AcceptRSP(const Row& row, CoreDriver& driver, Time at, const Response& response)
    {
        driver.TickTo(at);
        TIMING_CHECK(row, driver.impl.Push(driver.impl.rxrsp, response, EventKind::AcceptedRXRSP));
        driver.impl.Tick();
    }

    void AcceptDAT(const Row& row, CoreDriver& driver, Time at, const Data& data)
    {
        driver.TickTo(at);
        TIMING_CHECK(row, driver.impl.Push(driver.impl.rxdat, data, EventKind::AcceptedRXDAT));
        driver.impl.Tick();
    }

    void AcceptDAT(const Row& row, CoreDriver& driver, Time at, const std::vector<Data>& data)
    {
        driver.TickTo(at);
        for (const Data& beat : data)
            TIMING_CHECK(row, driver.impl.Push(driver.impl.rxdat, beat, EventKind::AcceptedRXDAT));
        driver.impl.Tick();
    }

    void AcceptCompAck(const Row& row, CoreDriver& driver, NodeId source, DataBufferId dataBufferId, Time at)
    {
        AcceptRSP(row, driver, at, MakeResponse(source, dataBufferId, CHI::Eb::Opcodes::RSP::CompAck));
    }

    Data PopDAT(const Row& row, CoreDriver& driver, Time popTime, Time visibleTime,
                uint16_t opcode = CHI::Eb::Opcodes::DAT::CompData, bool downstream = false)
    {
        Core::Impl& impl = driver.impl;
        driver.TickTo(popTime);
        bool found = false;
        for (const Pending<Data>& pending : impl.txdat)
            found |= pending.readyTime == visibleTime && pending.flit.opcode == opcode &&
                pending.downstream == downstream;
        TIMING_CHECK(row, found);
        const auto data = downstream
            ? impl.Pop(impl.txdat, EventKind::EmittedTXDAT, true)
            : impl.Pop(impl.txdat, EventKind::EmittedTXDAT);
        TIMING_CHECK(row, data && data->opcode == opcode);
        return *data;
    }

    Response PopRSP(const Row& row, CoreDriver& driver, Time popTime, Time visibleTime, uint16_t opcode)
    {
        Core::Impl& impl = driver.impl;
        driver.TickTo(popTime);
        bool found = false;
        for (const Pending<Response>& pending : impl.txrsp)
            found |= pending.readyTime == visibleTime && pending.flit.opcode == opcode;
        TIMING_CHECK(row, found);
        const auto response = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
        TIMING_CHECK(row, response && response->opcode == opcode);
        return *response;
    }

    Request PopREQ(const Row& row, CoreDriver& driver, Time popTime, Time visibleTime)
    {
        Core::Impl& impl = driver.impl;
        driver.TickTo(popTime);
        TIMING_CHECK(row, impl.txreq.size() == 1);
        ExpectTime(row, impl.txreq.front().readyTime, visibleTime, "downstream request visibility");
        const auto request = impl.Pop(impl.txreq, EventKind::EmittedTXREQ);
        TIMING_CHECK(row, request.has_value());
        return *request;
    }

    Snoop PopSNP(const Row& row, CoreDriver& driver, Time popTime, Time visibleTime, uint16_t opcode)
    {
        Core::Impl& impl = driver.impl;
        driver.TickTo(popTime);
        bool found = false;
        for (const Pending<Snoop>& pending : impl.txsnp)
            found |= pending.readyTime == visibleTime && pending.flit.opcode == opcode;
        TIMING_CHECK(row, found);
        const auto snoop = impl.Pop(impl.txsnp, EventKind::EmittedTXSNP);
        TIMING_CHECK(row, snoop && snoop->opcode == opcode);
        return *snoop;
    }

    void CheckBeat(const Row& row, const Data& data, const Options& options, size_t beat,
                   uint8_t resp, uint8_t respErr = 0, const LineData* expected = nullptr)
    {
        const size_t bytes = BytesPerBeat(options);
        const size_t offset = beat * bytes;
        ExpectTime(row, data.dataId, offset / 16, "return DataId");
        TIMING_CHECK(row, data.resp == resp);
        TIMING_CHECK(row, data.respErr == respErr);
        if (!expected)
            return;
        for (size_t index = 0; index < bytes; ++index)
            TIMING_CHECK(row, data.bytes[index] == (*expected)[offset + index]);
    }

    void ExpectRetired(const Row& row, CoreDriver& driver, TransactionHandle handle)
    {
        TIMING_CHECK(row, driver.impl.tracker.Find(handle) == nullptr);
        TIMING_CHECK(row, driver.impl.errors.empty());
        TIMING_CHECK(row, driver.impl.ValidateInvariants());
    }

    void RetireUnacked(const Row& row, CoreDriver& driver, TransactionHandle handle)
    {
        driver.impl.RetireFinished();
        ExpectRetired(row, driver, handle);
    }

    void AckAndRetire(const Row& row, CoreDriver& driver, const Transaction& transaction,
                      DataBufferId dataBufferId, Time at)
    {
        const TransactionHandle handle = transaction.handle;
        AcceptCompAck(row, driver, transaction.request.source, dataBufferId, at);
        ExpectRetired(row, driver, handle);
    }

    Request NonAllocating(Request request)
    {
        request.memAttr = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        if (ClassifyRequest(request.opcode) == RequestClass::NonCoherentRead)
            request.snpAttr = 0;
        return request;
    }

}

LUNATIC_TEST(Stage10MatrixR01InternalReadMemoryAndDataSchedule)
{
    for (const ProfileKind profile : Profiles)
        for (const bool delayed : { false, true })
        {
            const Row row{ profile, delayed ? "R01-delay" : "R01-zero" };
            Options options = ProfileOptions(profile);
            if (delayed)
            {
                options.timing.dataReturn = 3;
                options.timing.dataBeat = 2;
                options.timing.output = 2;
            }
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            const LineData payload = ReadPattern(11);
            const LineKey line = LineKey::FromAddress(0x100000, false);
            impl.memory.WriteFull(line, payload);
            Transaction& transaction = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp));
            const TransactionHandle handle = transaction.handle;
            driver.TickTo(delayed ? 20 : 17);
            TIMING_CHECK(row, transaction.phase == Phase::Return);
            ExpectTime(row, impl.flows.at(handle).scheduleMarks.at(SchedulePoint::DataReturn),
                delayed ? 20 : 17, "internal memory data return");
            driver.TickTo(delayed ? 25 : 17);
            TIMING_CHECK(row, !impl.Peek(impl.txdat));
            const Data first = PopDAT(row, driver, delayed ? 26 : 18, delayed ? 26 : 18);
            CheckBeat(row, first, options, 0, CHI::Eb::Resps::CompData_I, 0, &payload);
            if (delayed)
                ExpectTime(row, impl.flows.at(handle).scheduleMarks.at(SchedulePoint::DataBeat),
                    24, "second beat gate");
            const Data second = PopDAT(row, driver, delayed ? 29 : 19, delayed ? 29 : 19);
            CheckBeat(row, second, options, 1, CHI::Eb::Resps::CompData_I, 0, &payload);
            RetireUnacked(row, driver, handle);
        }
}

LUNATIC_TEST(Stage10MatrixR02OrdinaryChiReadDependencyPositions)
{
    for (const ProfileKind profile : Profiles)
        for (const Time dependency : { Time(10), Time(11), Time(15) })
        {
            const Row row{ profile, dependency == 10 ? "R02-E" : dependency == 11 ? "R02-S" : "R02-L4" };
            Options options = ProfileOptions(profile);
            options.backend = BackendKind::CHI;
            options.subordinateId = 6;
            options.dmtEnabled = false;
            options.timing.backendRequest = 2;
            options.timing.dataReturn = 3;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            const LineData payload = ReadPattern(23);
            Transaction& transaction = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp));
            const TransactionHandle handle = transaction.handle;
            driver.TickTo(8);
            TIMING_CHECK(row, !impl.Peek(impl.txreq));
            const Request downstream = PopREQ(row, driver, profile == ProfileKind::Type3 ? 9 : 10,
                profile == ProfileKind::Type3 ? 9 : 10);
            TIMING_CHECK(row, downstream.opcode == CHI::Eb::Opcodes::REQ::ReadNoSnp);
            TIMING_CHECK(row, downstream.returnNid == 2);
            AcceptDAT(row, driver, dependency, std::vector<Data>{
                BackendData(transaction, 0, options, payload),
                BackendData(transaction, 1, options, payload)
            });
            TIMING_CHECK(row, !impl.flows.at(handle).backendOperationOwned);
            const Time action = dependency + options.dataLatency + options.timing.dataReturn;
            driver.TickTo(action);
            TIMING_CHECK(row, transaction.phase == Phase::Return);
            ExpectTime(row, impl.flows.at(handle).scheduleMarks.at(SchedulePoint::DataReturn),
                action, "CHI backend data return");
            const Time firstAt = action + 5;
            const Data first = PopDAT(row, driver, firstAt, firstAt);
            CheckBeat(row, first, options, 0, CHI::Eb::Resps::CompData_I, 0, &payload);
            const Data second = PopDAT(row, driver, firstAt + 1, firstAt + 1);
            CheckBeat(row, second, options, 1, CHI::Eb::Resps::CompData_I, 0, &payload);
            RetireUnacked(row, driver, handle);
        }
}

LUNATIC_TEST(Stage10MatrixR03AxiReadSubmissionAndRLastPositions)
{
    for (const ProfileKind profile : Profiles)
        for (const Time dependency : { Time(7), Time(8), Time(12) })
        {
            const Row row{ profile, dependency == 7 ? "R03-E" : dependency == 8 ? "R03-S" : "R03-L4" };
            Options options = ProfileOptions(profile);
            options.backend = BackendKind::AXI;
            options.timing.dataReturn = 2;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            const LineData payload = ReadPattern(37);
            Transaction& transaction = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp));
            const TransactionHandle handle = transaction.handle;
            driver.TickTo(7);
            TIMING_CHECK(row, impl.txreq.empty());
            const auto address = impl.axi.PopAR();
            TIMING_CHECK(row, address && address->len == 1);
            driver.TickTo(dependency);
            for (size_t beat = 0; beat < 2; ++beat)
            {
                AXIReadData data;
                data.id = address->id;
                data.last = beat == 1;
                for (size_t index = 0; index < 32; ++index)
                    data.data[index] = payload[beat * 32 + index];
                TIMING_CHECK(row, impl.axi.PushR(data));
            }
            impl.Tick();
            TIMING_CHECK(row, !impl.flows.at(handle).backendOperationOwned);
            const Time action = dependency + options.dataLatency + options.timing.dataReturn;
            driver.TickTo(action);
            TIMING_CHECK(row, transaction.phase == Phase::Return);
            const Time firstAt = action + options.timing.dataReturn + 2;
            const Data first = PopDAT(row, driver, firstAt, firstAt);
            CheckBeat(row, first, options, 0, CHI::Eb::Resps::CompData_I, 0, &payload);
            const Data second = PopDAT(row, driver, firstAt + 1, firstAt + 1);
            CheckBeat(row, second, options, 1, CHI::Eb::Resps::CompData_I, 0, &payload);
            RetireUnacked(row, driver, handle);
            TIMING_CHECK(row, impl.txreq.empty());
        }
}

LUNATIC_TEST(Stage10MatrixR04OrderedReadReceiptAndIndependentDataPop)
{
    for (const ProfileKind profile : Profiles)
    {
        const Row row{ profile, "R04-H" };
        Options options = ProfileOptions(profile);
        options.dmtEnabled = false;
        options.timing.normalResponse = 2;
        options.timing.dataReturn = 1;
        options.timing.output = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        request.order = 2;
        Transaction& transaction = driver.Add(request);
        const TransactionHandle handle = transaction.handle;
        driver.TickTo(8);
        TIMING_CHECK(row, !impl.Peek(impl.txrsp));
        driver.TickTo(12);
        TIMING_CHECK(row, impl.txdat.empty());
        TIMING_CHECK(row, !impl.ordering.Empty());
        const Response receipt = PopRSP(row, driver, 13, profile == ProfileKind::Type3 ? 9 : 10,
            CHI::Eb::Opcodes::RSP::ReadReceipt);
        TIMING_CHECK(row, receipt.txnId == request.txnId);
        TIMING_CHECK(row, impl.ordering.Empty());
        TIMING_CHECK(row, impl.flows.at(handle).readReceiptEmitted);
        driver.TickTo(18);
        TIMING_CHECK(row, transaction.phase == Phase::Return);
        const Data first = PopDAT(row, driver, 21, 21);
        CheckBeat(row, first, options, 0, CHI::Eb::Resps::CompData_I);
        const Data second = PopDAT(row, driver, 22, 22);
        CheckBeat(row, second, options, 1, CHI::Eb::Resps::CompData_I);
        RetireUnacked(row, driver, handle);
    }
}

LUNATIC_TEST(Stage10MatrixR05SeparatedUpstreamResponseAndCompAckLegality)
{
    for (const ProfileKind profile : Profiles)
        for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::ReadNoSnp, CHI::Eb::Opcodes::REQ::ReadOnce })
            for (size_t mode = 0; mode < 3; ++mode)
            {
                const Row row{ profile, mode == 0 ? "R05-unordered" : mode == 1 ? "R05-early-ack" : "R05-ordered-ack" };
                Options options = ProfileOptions(profile);
                options.separatedReadResponse = true;
                options.timing.normalResponse = 1;
                options.timing.dataReturn = 3;
                options.timing.output = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                Request request = NonAllocating(Timing::MakeRequest(opcode));
                request.order = mode == 2 ? 2 : 0;
                request.expCompAck = mode != 0;
                Transaction& transaction = driver.Add(request);
                const TransactionHandle handle = transaction.handle;
                driver.TickTo(20);
                TIMING_CHECK(row, transaction.phase == Phase::Return);
                TIMING_CHECK(row, impl.flows.at(handle).separatedResponse);
                const Response response = PopRSP(row, driver, 23, 23, CHI::Eb::Opcodes::RSP::RespSepData);
                const DataBufferId dataBufferId = response.dbid;
                if (mode == 1)
                {
                    AcceptCompAck(row, driver, request.source, dataBufferId, 24);
                    TIMING_CHECK(row, impl.flows.at(handle).responseOwned);
                    TIMING_CHECK(row, transaction.phase == Phase::Return);
                }
                const Data first = PopDAT(row, driver, 25, 25, CHI::Eb::Opcodes::DAT::DataSepResp);
                CheckBeat(row, first, options, 0, CHI::Eb::Resps::DataSepResp_UC);
                if (mode == 2)
                {
                    AcceptCompAck(row, driver, request.source, dataBufferId, 25);
                    TIMING_CHECK(row, impl.flows.at(handle).responseOwned);
                }
                const Data second = PopDAT(row, driver, 26, 26, CHI::Eb::Opcodes::DAT::DataSepResp);
                CheckBeat(row, second, options, 1, CHI::Eb::Resps::DataSepResp_UC);
                RetireUnacked(row, driver, handle);
            }
}

LUNATIC_TEST(Stage10MatrixR06SeparatedBackendReceiptAndDataOrders)
{
    for (const ProfileKind profile : Profiles)
        for (size_t mode = 0; mode < 3; ++mode)
        {
            const Row row{ profile, mode == 0 ? "R06-receipt-first" : mode == 1 ? "R06-data-first" : "R06-same" };
            Options options = ProfileOptions(profile);
            options.backend = BackendKind::CHI;
            options.subordinateId = 6;
            options.separatedBackendRead = true;
            options.dmtEnabled = false;
            options.timing.backendRequest = 1;
            options.timing.normalResponse = 1;
            options.timing.dataReturn = 1;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            const LineData payload = ReadPattern(41);
            Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp);
            request.order = 2;
            request.expCompAck = true;
            Transaction& transaction = driver.Add(request);
            const TransactionHandle handle = transaction.handle;
            PopRSP(row, driver, profile == ProfileKind::Type3 ? 8 : 9,
                profile == ProfileKind::Type3 ? 8 : 9, CHI::Eb::Opcodes::RSP::ReadReceipt);
            const Request downstream = PopREQ(row, driver, profile == ProfileKind::Type3 ? 8 : 9, profile == ProfileKind::Type3 ? 8 : 9);
            TIMING_CHECK(row, downstream.opcode == CHI::Eb::Opcodes::REQ::ReadNoSnpSep);
            const Time receiptAt = mode == 0 ? 9 : mode == 1 ? 17 : 10;
            const Time dataAt = mode == 0 ? 15 : mode == 1 ? 9 : 10;
            if (receiptAt <= dataAt)
            {
                AcceptRSP(row, driver, receiptAt,
                    MakeResponse(6, transaction.handle, CHI::Eb::Opcodes::RSP::ReadReceipt));
                TIMING_CHECK(row, impl.flows.at(handle).backendReadReceiptReceived);
            }
            AcceptDAT(row, driver, dataAt, std::vector<Data>{
                BackendData(transaction, 0, options, payload, CHI::Eb::Opcodes::DAT::DataSepResp),
                BackendData(transaction, 1, options, payload, CHI::Eb::Opcodes::DAT::DataSepResp)
            });
            TIMING_CHECK(row, !impl.flows.at(handle).backendOperationOwned);
            const Time action = dataAt + 1 + options.dataLatency;
            driver.TickTo(action);
            TIMING_CHECK(row, transaction.phase == Phase::Return);
            const Time firstAt = action + 3;
            const Time secondAt = firstAt + 1;
            const Data first = PopDAT(row, driver, firstAt, firstAt);
            CheckBeat(row, first, options, 0, CHI::Eb::Resps::CompData_I, 0, &payload);
            AcceptCompAck(row, driver, request.source, first.dbid, firstAt);
            TIMING_CHECK(row, transaction.phase == Phase::Return);
            const Data second = PopDAT(row, driver, secondAt, secondAt);
            CheckBeat(row, second, options, 1, CHI::Eb::Resps::CompData_I, 0, &payload);
            if (receiptAt > dataAt)
            {
                TIMING_CHECK(row, !impl.flows.at(handle).backendReadReceiptReceived);
                impl.RetireFinished();
                TIMING_CHECK(row, impl.tracker.Find(handle) != nullptr);
                TIMING_CHECK(row, impl.errors.empty());
                AcceptRSP(row, driver, receiptAt,
                    MakeResponse(6, transaction.handle, CHI::Eb::Opcodes::RSP::ReadReceipt));
                ExpectRetired(row, driver, handle);
            }
            else
                RetireUnacked(row, driver, handle);
            TIMING_CHECK(row, impl.completedBackendReads.empty());
        }
}

LUNATIC_TEST(Stage10MatrixR07DmtDirectReturnAndLateReceipt)
{
    for (const ProfileKind profile : Profiles)
        for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::ReadNoSnp, CHI::Eb::Opcodes::REQ::ReadOnce })
            for (const Time dependency : { Time(10), Time(11), Time(15) })
            {
                const Row row{ profile, dependency == 10 ? "R07-E" : dependency == 11 ? "R07-S" : "R07-L4" };
                Options options = ProfileOptions(profile);
                options.backend = BackendKind::CHI;
                options.subordinateId = 6;
                options.dmtEnabled = true;
                options.timing.backendRequest = 1;
                options.timing.output = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                Request request = NonAllocating(Timing::MakeRequest(opcode));
                request.expCompAck = true;
                Transaction& transaction = driver.Add(request);
                const TransactionHandle handle = transaction.handle;
                TIMING_CHECK(row, impl.flows.at(handle).dmtActive);
                const Request downstream = PopREQ(row, driver, profile == ProfileKind::Type3 ? 8 : 9, profile == ProfileKind::Type3 ? 8 : 9);
                TIMING_CHECK(row, downstream.returnNid == request.source);
                TIMING_CHECK(row, downstream.returnTxnId == request.txnId);
                TIMING_CHECK(row, downstream.order == 1);
                TIMING_CHECK(row, impl.txdat.empty());
                TIMING_CHECK(row, impl.txrsp.empty());
                AcceptCompAck(row, driver, request.source, transaction.dbid, dependency);
                TIMING_CHECK(row, transaction.Complete(COMPLETE_RESPONSE));
                TIMING_CHECK(row, transaction.Complete(COMPLETE_COMPACK));
                TIMING_CHECK(row, transaction.phase == Phase::Commit);
                TIMING_CHECK(row, impl.tracker.Find(handle) == &transaction);
                TIMING_CHECK(row, impl.tracker.FindMemory(6, downstream.txnId) == &transaction);
                TIMING_CHECK(row, impl.completedBackendReads.empty());
                TIMING_CHECK(row, impl.txdat.empty());
                AcceptRSP(row, driver, dependency + 4,
                    MakeResponse(6, downstream.txnId, CHI::Eb::Opcodes::RSP::ReadReceipt));
                ExpectRetired(row, driver, handle);
                TIMING_CHECK(row, impl.completedBackendReads.empty());
                TIMING_CHECK(row, impl.errors.empty());
                TIMING_CHECK(row, impl.ValidateInvariants());
            }
}

LUNATIC_TEST(Stage10MatrixR08ReadOnceFamilyHitMissAndForwardingEligibility)
{
    const std::array<uint16_t, 3> opcodes = {
        CHI::Eb::Opcodes::REQ::ReadOnce,
        CHI::Eb::Opcodes::REQ::ReadOnceCleanInvalid,
        CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid
    };
    for (const ProfileKind profile : Profiles)
        for (const uint16_t opcode : opcodes)
            for (const bool homeHit : { false, true })
            {
                const Row row{ profile, homeHit ? "R08-hit" : "R08-miss" };
                Options options = ProfileOptions(profile);
                options.dctEnabled = profile == ProfileKind::Type7;
                options.timing.dataReturn = 1;
                options.timing.output = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                const LineKey line = LineKey::FromAddress(0x100000, false);
                const LineData payload = ReadPattern(53);
                if (homeHit)
                    SeedHome(impl, line, payload);
                else
                    impl.memory.WriteFull(line, payload);
                Transaction& transaction = driver.Add(NonAllocating(Timing::MakeRequest(opcode)));
                const TransactionHandle handle = transaction.handle;
                const bool dctEligible = profile == ProfileKind::Type7 &&
                    opcode != CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid;
                TIMING_CHECK(row, impl.flows.at(handle).dctActive == dctEligible);
                const Time action = homeHit ? 10 : 18;
                driver.TickTo(action);
                TIMING_CHECK(row, transaction.phase == Phase::Return);
                TIMING_CHECK(row, impl.txsnp.empty());
                TIMING_CHECK(row, impl.flows.at(handle).allocateHome == !homeHit);
                const uint8_t expectedResp = opcode == CHI::Eb::Opcodes::REQ::ReadOnce
                    ? uint8_t(CHI::Eb::Resps::CompData_UC) : uint8_t(CHI::Eb::Resps::CompData_I);
                const Time firstAt = homeHit ? 13 : 21;
                const Time secondAt = homeHit ? 14 : 22;
                const Data first = PopDAT(row, driver, firstAt, firstAt);
                CheckBeat(row, first, options, 0, expectedResp, 0, &payload);
                const Data second = PopDAT(row, driver, secondAt, secondAt);
                CheckBeat(row, second, options, 1, expectedResp, 0, &payload);
                RetireUnacked(row, driver, handle);
                TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) == CacheState::I);
            }
}

LUNATIC_TEST(Stage10MatrixR09CleanHomeHitHasNoSnoopAndExactDataReturn)
{
    const std::array<uint16_t, 3> opcodes = {
        CHI::Eb::Opcodes::REQ::ReadShared,
        CHI::Eb::Opcodes::REQ::ReadClean,
        CHI::Eb::Opcodes::REQ::ReadUnique
    };
    for (const ProfileKind profile : Profiles)
        for (const uint16_t opcode : opcodes)
        {
            const Row row{ profile, "R09" };
            Options options = ProfileOptions(profile);
            options.dctEnabled = profile == ProfileKind::Type7;
            options.timing.dataReturn = 2;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            const LineKey line = LineKey::FromAddress(0x100000, false);
            const LineData payload = ReadPattern(67);
            SeedHome(impl, line, payload);
            Transaction& transaction = driver.Add(Timing::MakeRequest(opcode));
            const TransactionHandle handle = transaction.handle;
            driver.TickTo(11);
            TIMING_CHECK(row, transaction.phase == Phase::Return);
            TIMING_CHECK(row, impl.txsnp.empty());
            TIMING_CHECK(row, transaction.snoopTargets.empty());
            ExpectTime(row, impl.flows.at(handle).scheduleMarks.at(SchedulePoint::DataReturn),
                11, "clean Home data return");
            const uint8_t expectedResp = opcode == CHI::Eb::Opcodes::REQ::ReadUnique
                ? uint8_t(CHI::Eb::Resps::CompData_UC) : uint8_t(CHI::Eb::Resps::CompData_SC);
            const Data first = PopDAT(row, driver, 15, 15);
            CheckBeat(row, first, options, 0, expectedResp, 0, &payload);
            const Data second = PopDAT(row, driver, 16, 16);
            CheckBeat(row, second, options, 1, expectedResp, 0, &payload);
            TIMING_CHECK(row, transaction.phase == Phase::WaitCompAck);
            TIMING_CHECK(row, impl.flows.at(handle).responseOwned);
            AckAndRetire(row, driver, transaction, second.dbid, 17);
            const CacheState expectedState = opcode == CHI::Eb::Opcodes::REQ::ReadUnique
                ? CacheState::UC : CacheState::SC;
            TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) == expectedState);
            TIMING_CHECK(row, impl.Bank(line).directory.Find(line) == nullptr);
        }
}

LUNATIC_TEST(Stage10MatrixR10CleanPeerResponseAndMemoryJoinPositions)
{
    for (const ProfileKind profile : Profiles)
        for (const Time dependency : { Time(11), Time(12), Time(16) })
        {
            const Row row{ profile, dependency == 11 ? "R10-E" : dependency == 12 ? "R10-S" : "R10-L4" };
            Options options = ProfileOptions(profile);
            options.dctEnabled = false;
            options.timing.snoopDecision = 1;
            options.timing.snoopIssue = 2;
            options.timing.dataReturn = 1;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            const LineKey line = LineKey::FromAddress(0x100000, false);
            const LineData payload = ReadPattern(79);
            impl.memory.WriteFull(line, payload);
            impl.Bank(line).snoopFilter.Track(line, 5, CacheState::SC, 0);
            Transaction& transaction = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadClean));
            const TransactionHandle handle = transaction.handle;
            driver.TickTo(6 + options.tagLatency);
            ExpectTime(row, impl.flows.at(handle).scheduleMarks.at(SchedulePoint::SnoopDecision),
                6 + options.tagLatency, "clean peer snoop decision");
            const Snoop snoop = PopSNP(row, driver, profile == ProfileKind::Type3 ? 10 : 11,
                profile == ProfileKind::Type3 ? 10 : 11, CHI::Eb::Opcodes::SNP::SnpClean);
            TIMING_CHECK(row, snoop.target == 5);
            TIMING_CHECK(row, impl.flows.at(handle).snoopWaveOwned);
            AcceptRSP(row, driver, dependency,
                MakeResponse(5, snoop.txnId, CHI::Eb::Opcodes::RSP::SnpResp, CHI::Eb::Resps::SnpResp_SC));
            TIMING_CHECK(row, !impl.flows.at(handle).snoopWaveOwned);
            TIMING_CHECK(row, transaction.memoryRequired);
            driver.TickTo(dependency + 8);
            TIMING_CHECK(row, !impl.flows.at(handle).backendOperationOwned);
            const Time action = dependency + 9 + options.dataLatency;
            driver.TickTo(action);
            TIMING_CHECK(row, transaction.phase == Phase::Return);
            const Time firstAt = action + options.timing.dataReturn + 2;
            const Data first = PopDAT(row, driver, firstAt, firstAt);
            CheckBeat(row, first, options, 0, CHI::Eb::Resps::CompData_SC, 0, &payload);
            const Data second = PopDAT(row, driver, firstAt + 1, firstAt + 1);
            CheckBeat(row, second, options, 1, CHI::Eb::Resps::CompData_SC, 0, &payload);
            AckAndRetire(row, driver, transaction, second.dbid, firstAt + 2);
            TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 5) == CacheState::SC);
        }
}

LUNATIC_TEST(Stage10MatrixR11DirtyPeerFullAndPartialDataJoins)
{
    for (const ProfileKind profile : Profiles)
        for (const bool partial : { false, true })
            for (const Time dependency : { Time(11), Time(12), Time(16) })
            {
                const Row row{ profile, dependency == 11 ? "R11-E" : dependency == 12 ? "R11-S" : "R11-L4" };
                Options options = ProfileOptions(profile);
                options.dctEnabled = false;
                options.timing.snoopIssue = 2;
                options.timing.dataReturn = 1;
                options.timing.output = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                const LineKey line = LineKey::FromAddress(0x100000, false);
                const LineData baseline = ReadPattern(83);
                const LineData peer = ReadPattern(131);
                LineData expected = peer;
                if (partial)
                    for (size_t index = 0; index < expected.size(); ++index)
                        if (index % 32 >= 16)
                            expected[index] = baseline[index];
                impl.memory.WriteFull(line, baseline);
                impl.Bank(line).snoopFilter.Track(line, 5, partial ? CacheState::UDP : CacheState::UD, 0);
                Transaction& transaction = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadUnique));
                const TransactionHandle handle = transaction.handle;
                const Snoop snoop = PopSNP(row, driver, profile == ProfileKind::Type3 ? 10 : 11,
                    profile == ProfileKind::Type3 ? 10 : 11, CHI::Eb::Opcodes::SNP::SnpUnique);
                const uint16_t opcode = partial ? CHI::Eb::Opcodes::DAT::SnpRespDataPtl
                                                : CHI::Eb::Opcodes::DAT::SnpRespData;
                const uint8_t resp = partial ? uint8_t(CHI::Eb::Resps::SnpRespDataPtl_I_PD)
                                             : uint8_t(CHI::Eb::Resps::SnpRespData_I_PD);
                const Data firstSnoop = MakeData(5, 2, snoop.txnId, 0, options, opcode, peer, resp, 0, partial);
                const Data lastSnoop = MakeData(5, 2, snoop.txnId, 1, options, opcode, peer, resp, 0, partial);
                if (dependency == 11)
                    AcceptDAT(row, driver, dependency, std::vector<Data>{ firstSnoop, lastSnoop });
                else
                {
                    AcceptDAT(row, driver, 11, firstSnoop);
                    TIMING_CHECK(row, transaction.pendingSnoops.size() == 1);
                    AcceptDAT(row, driver, dependency, lastSnoop);
                }
                TIMING_CHECK(row, transaction.pendingSnoops.empty());
                TIMING_CHECK(row, impl.flows.at(handle).snoopDataPartial == partial);
                TIMING_CHECK(row, !impl.flows.at(handle).snoopWaveOwned);
                const Time action = dependency + (partial ? 9 + options.dataLatency : 0);
                driver.TickTo(action);
                TIMING_CHECK(row, transaction.phase == Phase::Return);
                const Time firstAt = dependency + (partial ? 12 + options.dataLatency : 3);
                const Time secondAt = firstAt + 1;
                const Data first = PopDAT(row, driver, firstAt, firstAt);
                CheckBeat(row, first, options, 0, CHI::Eb::Resps::CompData_UD_PD, 0, &expected);
                const Data second = PopDAT(row, driver, secondAt, secondAt);
                CheckBeat(row, second, options, 1, CHI::Eb::Resps::CompData_UD_PD, 0, &expected);
                AckAndRetire(row, driver, transaction, second.dbid, secondAt + 1);
                TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 5) == CacheState::I);
                TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) ==
                    (partial ? CacheState::UDP : CacheState::UD));
            }
}

LUNATIC_TEST(Stage10MatrixR12DctProviderAndMultiOwnerFallback)
{
    for (const ProfileKind profile : Profiles)
    {
        for (const Time dependency : { Time(11), Time(12), Time(16) })
            for (const bool ackBeforeProvider : { false, true })
            {
                const Row row{ profile, dependency == 11 ? "R12-E" : dependency == 12 ? "R12-S" : "R12-L4" };
                Options options = ProfileOptions(profile);
                options.dctEnabled = true;
                options.timing.snoopDecision = 1;
                options.timing.snoopIssue = 2;
                options.timing.output = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                const LineKey line = LineKey::FromAddress(0x100000, false);
                impl.Bank(line).snoopFilter.Track(line, 5, CacheState::UC, 0);
                Transaction& transaction = driver.Add(NonAllocating(
                    Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadUnique)));
                const TransactionHandle handle = transaction.handle;
                const Snoop snoop = PopSNP(row, driver, profile == ProfileKind::Type3 ? 10 : 11,
                    profile == ProfileKind::Type3 ? 10 : 11, CHI::Eb::Opcodes::SNP::SnpUniqueFwd);
                TIMING_CHECK(row, snoop.target == 5);
                TIMING_CHECK(row, snoop.fwdNid == transaction.request.source);
                TIMING_CHECK(row, snoop.fwdTxnId == transaction.request.txnId);
                if (ackBeforeProvider)
                {
                    AcceptCompAck(row, driver, transaction.request.source, transaction.dbid, 11);
                    TIMING_CHECK(row, impl.tracker.Find(handle) != nullptr);
                }
                Response forwarded = MakeResponse(5, snoop.txnId, CHI::Eb::Opcodes::RSP::SnpRespFwded,
                    CHI::Eb::Resps::SnpResp_I);
                forwarded.fwdState = CHI::Eb::FwdStates::UC;
                AcceptRSP(row, driver, dependency, forwarded);
                TIMING_CHECK(row, impl.txdat.empty());
                if (ackBeforeProvider)
                    ExpectRetired(row, driver, handle);
                else
                {
                    TIMING_CHECK(row, transaction.phase == Phase::WaitCompAck);
                    AckAndRetire(row, driver, transaction, transaction.dbid, dependency + 1);
                }
                TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 5) == CacheState::I);
                TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) == CacheState::UC);
            }

        const Row row{ profile, "R12-multi-owner-fallback" };
        Options options = ProfileOptions(profile);
        options.dctEnabled = true;
        options.timing.snoopDecision = 1;
        options.timing.snoopIssue = 2;
        options.timing.dataReturn = 1;
        options.timing.output = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        const LineKey line = LineKey::FromAddress(0x100000, false);
        const LineData payload = ReadPattern(151);
        impl.memory.WriteFull(line, payload);
        impl.Bank(line).snoopFilter.Track(line, 5, CacheState::SC, 0);
        impl.Bank(line).snoopFilter.Track(line, 6, CacheState::SC, 0);
        Transaction& transaction = driver.Add(NonAllocating(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadUnique)));
        const TransactionHandle handle = transaction.handle;
        const Snoop first = PopSNP(row, driver, profile == ProfileKind::Type3 ? 10 : 11,
            profile == ProfileKind::Type3 ? 10 : 11, CHI::Eb::Opcodes::SNP::SnpUnique);
        TIMING_CHECK(row, !impl.flows.at(handle).dctActive);
        Snoop second;
        if (profile == ProfileKind::Type3)
        {
            impl.Tick();
            second = PopSNP(row, driver, 13, 13, CHI::Eb::Opcodes::SNP::SnpUnique);
        }
        else
            second = PopSNP(row, driver, 12, 12, CHI::Eb::Opcodes::SNP::SnpUnique);
        TIMING_CHECK(row, first.target != second.target);
        AcceptRSP(row, driver, 15,
            MakeResponse(first.target, first.txnId, CHI::Eb::Opcodes::RSP::SnpResp, CHI::Eb::Resps::SnpResp_I));
        TIMING_CHECK(row, transaction.pendingSnoops.size() == 1);
        AcceptRSP(row, driver, 15,
            MakeResponse(second.target, second.txnId, CHI::Eb::Opcodes::RSP::SnpResp, CHI::Eb::Resps::SnpResp_I));
        TIMING_CHECK(row, transaction.pendingSnoops.empty());
        const Time firstAt = profile == ProfileKind::Type3 ? 30 : 29;
        const Data firstData = PopDAT(row, driver, firstAt, firstAt);
        CheckBeat(row, firstData, options, 0, CHI::Eb::Resps::CompData_UC, 0, &payload);
        const Data secondData = PopDAT(row, driver, firstAt + 1, firstAt + 1);
        CheckBeat(row, secondData, options, 1, CHI::Eb::Resps::CompData_UC, 0, &payload);
        AckAndRetire(row, driver, transaction, secondData.dbid, firstAt + 2);
        TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 5) == CacheState::I);
        TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 6) == CacheState::I);
    }
}

LUNATIC_TEST(Stage10MatrixR13ReadUniqueAndMakeUniqueInvalidationWaves)
{
    for (const ProfileKind profile : Profiles)
        for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::ReadUnique, CHI::Eb::Opcodes::REQ::MakeUnique })
            for (size_t sharers = 0; sharers <= 2; ++sharers)
                for (const Time dependency : { Time(10), Time(11), Time(15) })
                {
                    if (sharers != 1 && dependency != 15)
                        continue;
                    const Row row{ profile, dependency == 10 ? "R13-E" : dependency == 11 ? "R13-S" : "R13-L4" };
                    Options options = ProfileOptions(profile);
                    options.dctEnabled = false;
                    options.timing.snoopIssue = 1;
                    options.timing.normalResponse = 1;
                    options.timing.dataReturn = 1;
                    options.timing.output = 1;
                    CoreDriver driver(options);
                    Core::Impl& impl = driver.impl;
                    const LineKey line = LineKey::FromAddress(0x100000, false);
                    const LineData payload = ReadPattern(163);
                    impl.memory.WriteFull(line, payload);
                    for (size_t index = 0; index < sharers; ++index)
                        impl.Bank(line).snoopFilter.Track(line, 5 + index, CacheState::SC, 0);
                    Transaction& transaction = driver.Add(Timing::MakeRequest(opcode));
                    const bool makeUnique = opcode == CHI::Eb::Opcodes::REQ::MakeUnique;
                    if (sharers == 0)
                    {
                        if (makeUnique)
                        {
                            driver.TickTo(9);
                            const Response comp = PopRSP(row, driver, 11, 11, CHI::Eb::Opcodes::RSP::Comp);
                            TIMING_CHECK(row, comp.resp == CHI::Eb::Resps::Comp_UC);
                            TIMING_CHECK(row, impl.txdat.empty());
                            AckAndRetire(row, driver, transaction, comp.dbid, 12);
                        }
                        else
                        {
                            const Data first = PopDAT(row, driver, 21, 21);
                            CheckBeat(row, first, options, 0, CHI::Eb::Resps::CompData_UC, 0, &payload);
                            const Data second = PopDAT(row, driver, 22, 22);
                            CheckBeat(row, second, options, 1, CHI::Eb::Resps::CompData_UC, 0, &payload);
                            AckAndRetire(row, driver, transaction, second.dbid, 23);
                        }
                    }
                    else
                    {
                        const Snoop first = PopSNP(row, driver, profile == ProfileKind::Type3 ? 9 : 10,
                            profile == ProfileKind::Type3 ? 9 : 10, CHI::Eb::Opcodes::SNP::SnpUnique);
                        std::vector<Snoop> snoops{ first };
                        if (sharers == 2)
                        {
                            if (profile == ProfileKind::Type3)
                            {
                                impl.Tick();
                                snoops.push_back(PopSNP(row, driver, 11, 11, CHI::Eb::Opcodes::SNP::SnpUnique));
                            }
                            else
                                snoops.push_back(PopSNP(row, driver, 11, 11, CHI::Eb::Opcodes::SNP::SnpUnique));
                        }
                        for (const Snoop& snoop : snoops)
                            AcceptRSP(row, driver, dependency, MakeResponse(snoop.target, snoop.txnId,
                                CHI::Eb::Opcodes::RSP::SnpResp, CHI::Eb::Resps::SnpResp_I));
                        TIMING_CHECK(row, transaction.pendingSnoops.empty());
                        if (makeUnique)
                        {
                            const Response comp = PopRSP(row, driver, dependency + 2, dependency + 2,
                                CHI::Eb::Opcodes::RSP::Comp);
                            TIMING_CHECK(row, comp.resp == CHI::Eb::Resps::Comp_UC);
                            TIMING_CHECK(row, impl.txdat.empty());
                            AckAndRetire(row, driver, transaction, comp.dbid, dependency + 3);
                        }
                        else
                        {
                            const Time firstAt = dependency + (profile == ProfileKind::Type3 ? 15 : 14);
                            const Data firstData = PopDAT(row, driver, firstAt, firstAt);
                            CheckBeat(row, firstData, options, 0, CHI::Eb::Resps::CompData_UC, 0, &payload);
                            const Data secondData = PopDAT(row, driver, firstAt + 1, firstAt + 1);
                            CheckBeat(row, secondData, options, 1, CHI::Eb::Resps::CompData_UC, 0, &payload);
                            AckAndRetire(row, driver, transaction, secondData.dbid, firstAt + 2);
                        }
                    }
                    TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) ==
                        (makeUnique ? CacheState::UD : CacheState::UC));
                    for (size_t index = 0; index < sharers; ++index)
                        TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 5 + index) == CacheState::I);
                    TIMING_CHECK(row, impl.txdat.empty());
                }
}

LUNATIC_TEST(Stage10MatrixR14MakeReadUniqueRequesterStateSelectsResponseForm)
{
    const std::array<CacheState, 3> states = { CacheState::I, CacheState::SC, CacheState::SD };
    for (const ProfileKind profile : Profiles)
        for (const CacheState initial : states)
        {
            const Row row{ profile, initial == CacheState::I ? "R14-invalid" :
                initial == CacheState::SC ? "R14-shared" : "R14-dirty" };
            Options options = ProfileOptions(profile);
            options.timing.normalResponse = 1;
            options.timing.dataReturn = 1;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            const LineKey line = LineKey::FromAddress(0x100000, false);
            const LineData payload = ReadPattern(179);
            SeedHome(impl, line, payload, initial == CacheState::SD);
            impl.Bank(line).snoopFilter.Track(line, 4, initial, 0);
            Transaction& transaction = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::MakeReadUnique));
            driver.TickTo(10);
            TIMING_CHECK(row, transaction.phase == Phase::Return);
            if (initial == CacheState::I)
            {
                const Data first = PopDAT(row, driver, 13, 13);
                CheckBeat(row, first, options, 0, CHI::Eb::Resps::CompData_UC, 0, &payload);
                const Data second = PopDAT(row, driver, 14, 14);
                CheckBeat(row, second, options, 1, CHI::Eb::Resps::CompData_UC, 0, &payload);
                AckAndRetire(row, driver, transaction, second.dbid, 15);
            }
            else
            {
                const uint8_t expectedResp = initial == CacheState::SD
                    ? uint8_t(CHI::Eb::Resps::Comp_UD_PD) : uint8_t(CHI::Eb::Resps::Comp_UC);
                const Response comp = PopRSP(row, driver, 12, 12, CHI::Eb::Opcodes::RSP::Comp);
                TIMING_CHECK(row, comp.resp == expectedResp);
                TIMING_CHECK(row, impl.txdat.empty());
                AckAndRetire(row, driver, transaction, comp.dbid, 13);
            }
            const CacheState expectedState = initial == CacheState::SD ? CacheState::UD : CacheState::UC;
            TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) == expectedState);
        }
}

LUNATIC_TEST(Stage10MatrixR15SharedDirtyAndPreferUniqueSourceJoins)
{
    const std::array<uint16_t, 2> opcodes = {
        CHI::Eb::Opcodes::REQ::ReadNotSharedDirty,
        CHI::Eb::Opcodes::REQ::ReadPreferUnique
    };
    const std::array<CacheState, 3> states = { CacheState::SC, CacheState::SD, CacheState::UC };
    for (const ProfileKind profile : Profiles)
        for (const uint16_t opcode : opcodes)
            for (const CacheState initial : states)
                for (const Time dependency : { Time(10), Time(11), Time(15) })
                {
                    const Row row{ profile, dependency == 10 ? "R15-E" : dependency == 11 ? "R15-S" : "R15-L4" };
                    Options options = ProfileOptions(profile);
                    options.dctEnabled = profile == ProfileKind::Type7;
                    options.timing.snoopIssue = 1;
                    options.timing.dataReturn = 1;
                    options.timing.output = 1;
                    CoreDriver driver(options);
                    Core::Impl& impl = driver.impl;
                    const LineKey line = LineKey::FromAddress(0x100000, false);
                    const LineData peer = ReadPattern(191);
                    const LineData memory = ReadPattern(197);
                    impl.memory.WriteFull(line, memory);
                    impl.Bank(line).snoopFilter.Track(line, 5, initial, 0);
                    Transaction& transaction = driver.Add(Timing::MakeRequest(opcode));
                    const TransactionHandle handle = transaction.handle;
                    TIMING_CHECK(row, !impl.flows.at(handle).dctActive);
                    const uint16_t snoopOpcode = opcode == CHI::Eb::Opcodes::REQ::ReadNotSharedDirty
                        ? uint16_t(CHI::Eb::Opcodes::SNP::SnpNotSharedDirty)
                        : uint16_t(CHI::Eb::Opcodes::SNP::SnpPreferUnique);
                    const Snoop snoop = PopSNP(row, driver, profile == ProfileKind::Type3 ? 9 : 10,
                        profile == ProfileKind::Type3 ? 9 : 10, snoopOpcode);
                    const bool memoryJoin = initial == CacheState::SC;
                    if (memoryJoin)
                    {
                        AcceptRSP(row, driver, dependency, MakeResponse(5, snoop.txnId,
                            CHI::Eb::Opcodes::RSP::SnpResp, CHI::Eb::Resps::SnpResp_SC));
                        TIMING_CHECK(row, transaction.memoryRequired);
                    }
                    else
                    {
                        const uint8_t snoopResp = initial == CacheState::SD
                            ? uint8_t(CHI::Eb::Resps::SnpRespData_SC_PD) : uint8_t(CHI::Eb::Resps::SnpRespData_SC);
                        const Data firstSnoop = MakeData(5, 2, snoop.txnId, 0, options,
                            CHI::Eb::Opcodes::DAT::SnpRespData, peer, snoopResp);
                        const Data lastSnoop = MakeData(5, 2, snoop.txnId, 1, options,
                            CHI::Eb::Opcodes::DAT::SnpRespData, peer, snoopResp);
                        if (dependency == 10)
                            AcceptDAT(row, driver, dependency, std::vector<Data>{ firstSnoop, lastSnoop });
                        else
                        {
                            AcceptDAT(row, driver, 10, firstSnoop);
                            AcceptDAT(row, driver, dependency, lastSnoop);
                        }
                        TIMING_CHECK(row, !transaction.memoryRequired);
                    }
                    const Time action = memoryJoin ? dependency + 9 + options.dataLatency : dependency;
                    driver.TickTo(action);
                    TIMING_CHECK(row, transaction.phase == Phase::Return);
                    const bool expectedAllocation = profile == ProfileKind::Type3 &&
                        opcode == CHI::Eb::Opcodes::REQ::ReadNotSharedDirty;
                    TIMING_CHECK(row, impl.flows.at(handle).allocateHome == expectedAllocation);
                    TIMING_CHECK(row, impl.flows.at(handle).requesterFinal == CacheState::SC);
                    const Time firstAt = memoryJoin ? action + options.timing.dataReturn + 2 : dependency + 3;
                    const LineData& expected = memoryJoin ? memory : peer;
                    const Data first = PopDAT(row, driver, firstAt, firstAt);
                    CheckBeat(row, first, options, 0, CHI::Eb::Resps::CompData_SC, 0, &expected);
                    const Data second = PopDAT(row, driver, firstAt + 1, firstAt + 1);
                    CheckBeat(row, second, options, 1, CHI::Eb::Resps::CompData_SC, 0, &expected);
                    AckAndRetire(row, driver, transaction, second.dbid, firstAt + 2);
                    TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 4) == CacheState::SC);
                    TIMING_CHECK(row, impl.Bank(line).snoopFilter.State(line, 5) == CacheState::SC);
                    const HomeLine* home = impl.Bank(line).directory.Find(line);
                    TIMING_CHECK(row, (home != nullptr) == expectedAllocation);
                }
}

LUNATIC_TEST(Stage10MatrixR16RefillCleanAndDirtyVictimJoins)
{
    for (const ProfileKind profile : Profiles)
        for (const bool dirtyVictim : { false, true })
            for (const Time offset : { Time(0), Time(1), Time(5) })
            {
                const Row row{ profile, offset == 0 ? "R16-E" : offset == 1 ? "R16-S" : "R16-L4" };
                Options options = ProfileOptions(profile);
                options.backend = BackendKind::CHI;
                options.subordinateId = 6;
                options.policy = PolicyKind::Inclusive;
                options.slcSets = 1;
                options.slcWays = 1;
                options.dctEnabled = false;
                options.dmtEnabled = false;
                options.timing.backendRequest = 1;
                options.timing.backendData = 1;
                options.timing.snoopIssue = 1;
                options.timing.dataReturn = 1;
                options.timing.output = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                const LineKey victimLine = LineKey::FromAddress(0x100000, false);
                const LineKey readLine = LineKey::FromAddress(
                    victimLine.address + options.structure.bankCount * LINE_SIZE, false);
                const LineData victimPayload = ReadPattern(211);
                const LineData readPayload = ReadPattern(223);
                SeedHome(impl, victimLine, victimPayload, dirtyVictim);
                impl.Bank(victimLine).snoopFilter.Track(victimLine, 5,
                    dirtyVictim ? CacheState::UD : CacheState::SC, 0);
                Transaction& transaction = driver.Add(Timing::MakeRequest(
                    CHI::Eb::Opcodes::REQ::ReadUnique, 4, 1, readLine.address));
                const TransactionHandle handle = transaction.handle;
                const Request baseline = PopREQ(row, driver, profile == ProfileKind::Type3 ? 8 : 9, profile == ProfileKind::Type3 ? 8 : 9);
                TIMING_CHECK(row, baseline.opcode == CHI::Eb::Opcodes::REQ::ReadNoSnp);
                const Time baselineAt = 7 + options.tagLatency + offset;
                AcceptDAT(row, driver, baselineAt, std::vector<Data>{
                    BackendData(transaction, 0, options, readPayload),
                    BackendData(transaction, 1, options, readPayload)
                });
                TIMING_CHECK(row, !impl.flows.at(handle).backendOperationOwned);
                const Time action = baselineAt + 1 + options.dataLatency;
                driver.TickTo(action);
                TIMING_CHECK(row, impl.flows.at(handle).victimPending);
                TIMING_CHECK(row, !impl.flows.at(handle).directoryCommitted);
                const Time snoopVisible = action + 3;
                const Snoop victim = PopSNP(row, driver, snoopVisible, snoopVisible,
                    CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
                TIMING_CHECK(row, victim.address == victimLine.address);
                const Time victimAt = snoopVisible + offset;
                if (dirtyVictim)
                {
                    const Data first = MakeData(5, 2, victim.txnId, 0, options,
                        CHI::Eb::Opcodes::DAT::SnpRespData, victimPayload, CHI::Eb::Resps::SnpRespData_I_PD);
                    const Data last = MakeData(5, 2, victim.txnId, 1, options,
                        CHI::Eb::Opcodes::DAT::SnpRespData, victimPayload, CHI::Eb::Resps::SnpRespData_I_PD);
                    if (victimAt == snoopVisible)
                        AcceptDAT(row, driver, victimAt, std::vector<Data>{ first, last });
                    else
                    {
                        AcceptDAT(row, driver, snoopVisible, first);
                        AcceptDAT(row, driver, victimAt, last);
                    }
                    TIMING_CHECK(row, impl.flows.at(handle).spillPending);
                    const Request spill = PopREQ(row, driver, victimAt + 2, victimAt + 2);
                    TIMING_CHECK(row, spill.opcode == CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
                    TIMING_CHECK(row, spill.address == victimLine.address);
                    Response dbid = MakeResponse(6, transaction.handle, CHI::Eb::Opcodes::RSP::DBIDResp);
                    dbid.dbid = 77;
                    AcceptRSP(row, driver, victimAt + 2, dbid);
                    const Data firstSpill = PopDAT(row, driver, victimAt + 4, victimAt + 4,
                        CHI::Eb::Opcodes::DAT::NonCopyBackWrData, true);
                    CheckBeat(row, firstSpill, options, 0, 0, 0, &victimPayload);
                    const Data lastSpill = PopDAT(row, driver, victimAt + 5, victimAt + 5,
                        CHI::Eb::Opcodes::DAT::NonCopyBackWrData, true);
                    CheckBeat(row, lastSpill, options, 1, 0, 0, &victimPayload);
                    AcceptRSP(row, driver, victimAt + 6,
                        MakeResponse(6, transaction.handle, CHI::Eb::Opcodes::RSP::Comp));
                    TIMING_CHECK(row, !impl.flows.at(handle).backendOperationOwned);
                    TIMING_CHECK(row, !impl.flows.at(handle).spillPending);
                }
                else
                    AcceptRSP(row, driver, victimAt, MakeResponse(5, victim.txnId,
                        CHI::Eb::Opcodes::RSP::SnpResp, CHI::Eb::Resps::SnpResp_I));
                TIMING_CHECK(row, transaction.phase == Phase::Return);
                TIMING_CHECK(row, impl.flows.at(handle).directoryCommitted);
                const Time firstAt = victimAt + (dirtyVictim ? 9 : 3);
                const Time secondAt = victimAt + (dirtyVictim ? 10 : 4);
                const Data first = PopDAT(row, driver, firstAt, firstAt);
                CheckBeat(row, first, options, 0, CHI::Eb::Resps::CompData_UC, 0, &readPayload);
                const Data second = PopDAT(row, driver, secondAt, secondAt);
                CheckBeat(row, second, options, 1, CHI::Eb::Resps::CompData_UC, 0, &readPayload);
                AckAndRetire(row, driver, transaction, second.dbid, secondAt + 1);
                TIMING_CHECK(row, impl.Bank(victimLine).directory.Find(victimLine) == nullptr);
                const HomeLine* installed = impl.Bank(readLine).directory.Find(readLine);
                TIMING_CHECK(row, installed && installed->valid && installed->data == readPayload);
                TIMING_CHECK(row, impl.Bank(victimLine).snoopFilter.State(victimLine, 5) == CacheState::I);
                TIMING_CHECK(row, impl.Bank(readLine).snoopFilter.State(readLine, 4) == CacheState::UC);
                if (dirtyVictim)
                    TIMING_CHECK(row, impl.memory.Read(victimLine) == victimPayload);
            }
}

LUNATIC_TEST(Stage10MatrixR17BeatIntervalsAndHeldOutput)
{
    for (const ProfileKind profile : Profiles)
        for (const size_t dataWidth : { size_t(256), size_t(128) })
        {
            const Row row{ profile, dataWidth == 256 ? "R17-two-beat" : "R17-four-beat" };
            Options options = ProfileOptions(profile);
            options.dataWidth = dataWidth;
            options.timing.dataReturn = 1;
            options.timing.dataBeat = 2;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            const LineKey line = LineKey::FromAddress(0x100000, false);
            const LineData payload = ReadPattern(227);
            impl.memory.WriteFull(line, payload);
            Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp);
            request.expCompAck = true;
            Transaction& transaction = driver.Add(request);
            const TransactionHandle handle = transaction.handle;
            const size_t beats = BeatsPerLine(options);
            driver.TickTo(21);
            const Data* firstPeek = impl.Peek(impl.txdat);
            TIMING_CHECK(row, firstPeek && firstPeek->dataId == 0);
            const DataBufferId dataBufferId = firstPeek->dbid;
            TIMING_CHECK(row, impl.Peek(impl.txdat) == firstPeek);
            ExpectTime(row, impl.txdat.front().readyTime, 21, "first beat visibility");
            driver.TickTo(23);
            ExpectTime(row, impl.txdat.front().readyTime, 21, "held first beat ready time");
            TIMING_CHECK(row, impl.txdat.size() >= 2);
            ExpectTime(row, impl.txdat[1].readyTime, 24, "second beat held ready time");
            ExpectTime(row, impl.flows.at(handle).scheduleMarks.at(SchedulePoint::DataBeat),
                dataWidth == 128 ? 25 : 22, "latest queued beat schedule mark");
            const Data first = PopDAT(row, driver, 24, 21);
            CheckBeat(row, first, options, 0, CHI::Eb::Resps::CompData_I, 0, &payload);
            TIMING_CHECK(row, first.dbid == dataBufferId);
            for (size_t beat = 1; beat < beats; ++beat)
            {
                const Time visible = beat == 1 ? 24 : beat == 2 ? 27 : 30;
                const Data data = PopDAT(row, driver, visible, visible);
                CheckBeat(row, data, options, beat, CHI::Eb::Resps::CompData_I, 0, &payload);
                TIMING_CHECK(row, data.dbid == dataBufferId);
            }
            TIMING_CHECK(row, impl.flows.at(handle).dataBeatScheduled == beats - 1);
            TIMING_CHECK(row, transaction.phase == Phase::WaitCompAck);
            AckAndRetire(row, driver, transaction, dataBufferId, beats == 2 ? 25 : 31);
        }
}

LUNATIC_TEST(Stage10MatrixR18CompAckEarlySameLateAndPreDataRejection)
{
    for (const ProfileKind profile : Profiles)
    {
        for (size_t position = 0; position < 3; ++position)
        {
            const Row row{ profile, position == 0 ? "R18-E" : position == 1 ? "R18-S" : "R18-L4" };
            Options options = ProfileOptions(profile);
            options.timing.normalResponse = 1;
            options.timing.dataReturn = 1;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp);
            request.order = 2;
            request.expCompAck = true;
            Transaction& transaction = driver.Add(request);
            const TransactionHandle handle = transaction.handle;
            PopRSP(row, driver, profile == ProfileKind::Type3 ? 8 : 9,
                profile == ProfileKind::Type3 ? 8 : 9, CHI::Eb::Opcodes::RSP::ReadReceipt);
            const Data first = PopDAT(row, driver, 21, 21);
            CheckBeat(row, first, options, 0, CHI::Eb::Resps::CompData_I);
            if (position == 0)
            {
                AcceptCompAck(row, driver, request.source, first.dbid, 21);
                TIMING_CHECK(row, impl.tracker.Find(handle) != nullptr);
                TIMING_CHECK(row, transaction.phase == Phase::Return);
            }
            const Data second = PopDAT(row, driver, 22, 22);
            CheckBeat(row, second, options, 1, CHI::Eb::Resps::CompData_I);
            TIMING_CHECK(row, second.dbid == first.dbid);
            if (position == 0)
                RetireUnacked(row, driver, handle);
            else
                AckAndRetire(row, driver, transaction, second.dbid, position == 1 ? 22 : 26);
        }

        const Row row{ profile, "R18-pre-data-rejection" };
        Options options = ProfileOptions(profile);
        options.timing.normalResponse = 1;
        options.timing.output = 1;
        CoreDriver driver(options);
        Core::Impl& impl = driver.impl;
        Request request = Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        request.order = 2;
        request.expCompAck = true;
        Transaction& transaction = driver.Add(request);
        const TransactionHandle handle = transaction.handle;
        PopRSP(row, driver, profile == ProfileKind::Type3 ? 8 : 9,
                profile == ProfileKind::Type3 ? 8 : 9, CHI::Eb::Opcodes::RSP::ReadReceipt);
        AcceptCompAck(row, driver, request.source, transaction.dbid, 10);
        TIMING_CHECK(row, impl.errors.size() == 1);
        TIMING_CHECK(row, impl.tracker.Find(handle) != nullptr);
        TIMING_CHECK(row, !transaction.Complete(COMPLETE_COMPACK));
        impl.Reset();
        TIMING_CHECK(row, impl.ValidateInvariants());
    }
}

LUNATIC_TEST(Stage10MatrixR19NarrowReadDataIdMasks)
{
    for (const ProfileKind profile : Profiles)
        for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::ReadNoSnp, CHI::Eb::Opcodes::REQ::ReadOnce })
            for (size_t alignment = 0; alignment < 4; ++alignment)
            {
                const Row row{ profile, "R19" };
                Options options = ProfileOptions(profile);
                options.dataWidth = 128;
                options.timing.dataReturn = 2;
                options.timing.dataBeat = 3;
                options.timing.output = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                const LineKey line = LineKey::FromAddress(0x100000, false);
                const LineData payload = ReadPattern(233);
                impl.memory.WriteFull(line, payload);
                Request request = Timing::MakeRequest(opcode, 4, 1, line.address + alignment * 16);
                request.size = 4;
                Transaction& transaction = driver.Add(request);
                const TransactionHandle handle = transaction.handle;
                driver.TickTo(7);
                TIMING_CHECK(row, impl.flows.at(handle).expectedBeatMask == (uint64_t(1) << alignment));
                driver.TickTo(19);
                TIMING_CHECK(row, transaction.phase == Phase::Return);
                const uint8_t expectedResp = opcode == CHI::Eb::Opcodes::REQ::ReadOnce
                    ? uint8_t(CHI::Eb::Resps::CompData_UC) : uint8_t(CHI::Eb::Resps::CompData_I);
                const Data data = PopDAT(row, driver, 23, 23);
                CheckBeat(row, data, options, alignment, expectedResp, 0, &payload);
                TIMING_CHECK(row, data.ccid == alignment);
                for (size_t index = 0; index < 16; ++index)
                    TIMING_CHECK(row, data.byteEnables[index] == 1);
                TIMING_CHECK(row, !impl.flows.at(handle).scheduleMarks.contains(SchedulePoint::DataBeat));
                TIMING_CHECK(row, impl.txdat.empty());
                RetireUnacked(row, driver, handle);
            }
}

LUNATIC_TEST(Stage10MatrixR20BackendErrorPropagationAndOwnership)
{
    for (const ProfileKind profile : Profiles)
    {
        for (const bool separated : { false, true })
            for (size_t errorCase = 0; errorCase < 2; ++errorCase)
            {
                const Row row{ profile, errorCase == 0 ? "R20-first-DERR" : "R20-last-NDERR" };
                Options options = ProfileOptions(profile);
                options.backend = BackendKind::CHI;
                options.subordinateId = 6;
                options.dmtEnabled = false;
                options.separatedReadResponse = separated;
                options.timing.backendRequest = 1;
                options.timing.normalResponse = 1;
                options.timing.dataReturn = 1;
                options.timing.output = 1;
                CoreDriver driver(options);
                Core::Impl& impl = driver.impl;
                Transaction& transaction = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp));
                const TransactionHandle handle = transaction.handle;
                PopREQ(row, driver, profile == ProfileKind::Type3 ? 8 : 9, profile == ProfileKind::Type3 ? 8 : 9);
                const LineData payload = ReadPattern(239);
                const uint8_t firstError = errorCase == 0 ? uint8_t(CHI::Eb::RespErrs::DERR)
                                                         : uint8_t(CHI::Eb::RespErrs::OK);
                const uint8_t lastError = errorCase == 0 ? uint8_t(CHI::Eb::RespErrs::OK)
                                                        : uint8_t(CHI::Eb::RespErrs::NDERR);
                AcceptDAT(row, driver, 10, BackendData(transaction, 0, options, payload,
                    CHI::Eb::Opcodes::DAT::CompData, firstError));
                TIMING_CHECK(row, impl.flows.at(handle).backendOperationOwned);
                AcceptDAT(row, driver, 11, BackendData(transaction, 1, options, payload,
                    CHI::Eb::Opcodes::DAT::CompData, lastError));
                TIMING_CHECK(row, !impl.flows.at(handle).backendOperationOwned);
                const uint8_t expectedFirstError = errorCase == 0 ? uint8_t(CHI::Eb::RespErrs::DERR)
                                                                  : uint8_t(CHI::Eb::RespErrs::NDERR);
                const uint8_t expectedLastError = errorCase == 0 ? uint8_t(CHI::Eb::RespErrs::OK)
                                                                 : uint8_t(CHI::Eb::RespErrs::NDERR);
                const uint16_t dataOpcode = separated ? CHI::Eb::Opcodes::DAT::DataSepResp
                                                      : CHI::Eb::Opcodes::DAT::CompData;
                const uint8_t expectedResp = separated ? uint8_t(CHI::Eb::Resps::DataSepResp_UC)
                                                       : uint8_t(CHI::Eb::Resps::CompData_I);
                const uint8_t expectedResponseError = firstError == CHI::Eb::RespErrs::NDERR ||
                    lastError == CHI::Eb::RespErrs::NDERR ? uint8_t(CHI::Eb::RespErrs::NDERR)
                                                          : uint8_t(CHI::Eb::RespErrs::OK);
                if (separated)
                {
                    const Response response = PopRSP(row, driver, profile == ProfileKind::Type3 ? 18 : 17,
                        profile == ProfileKind::Type3 ? 18 : 17, CHI::Eb::Opcodes::RSP::RespSepData);
                    TIMING_CHECK(row, response.respErr == expectedResponseError);
                }
                const Time firstAt = profile == ProfileKind::Type3 ? 18 : 17;
                const Data first = PopDAT(row, driver, firstAt, firstAt, dataOpcode);
                CheckBeat(row, first, options, 0, expectedResp, expectedFirstError, &payload);
                TIMING_CHECK(row, impl.flows.at(handle).responseOwned);
                const Data second = PopDAT(row, driver, firstAt + 1, firstAt + 1, dataOpcode);
                CheckBeat(row, second, options, 1, expectedResp, expectedLastError, &payload);
                RetireUnacked(row, driver, handle);
            }

        for (const AXIResp responseError : { AXIResp::SLVERR, AXIResp::DECERR })
        {
            const Row row{ profile, responseError == AXIResp::SLVERR ? "R20-AXI-SLVERR" : "R20-AXI-DECERR" };
            Options options = ProfileOptions(profile);
            options.backend = BackendKind::AXI;
            options.timing.dataReturn = 1;
            options.timing.output = 1;
            CoreDriver driver(options);
            Core::Impl& impl = driver.impl;
            Transaction& transaction = driver.Add(Timing::MakeRequest(CHI::Eb::Opcodes::REQ::ReadNoSnp));
            const TransactionHandle handle = transaction.handle;
            driver.TickTo(7);
            const auto address = impl.axi.PopAR();
            TIMING_CHECK(row, address && address->len == 1);
            AXIReadData first;
            first.id = address->id;
            first.resp = responseError;
            TIMING_CHECK(row, impl.axi.PushR(first));
            impl.Tick();
            TIMING_CHECK(row, impl.flows.at(handle).backendOperationOwned);
            AXIReadData last;
            last.id = address->id;
            last.last = true;
            driver.TickTo(9);
            TIMING_CHECK(row, impl.axi.PushR(last));
            impl.Tick();
            TIMING_CHECK(row, !impl.flows.at(handle).backendOperationOwned);
            const Time firstAt = 13 + options.dataLatency;
            const Data firstData = PopDAT(row, driver, firstAt, firstAt);
            CheckBeat(row, firstData, options, 0, CHI::Eb::Resps::CompData_I, CHI::Eb::RespErrs::NDERR);
            const Data lastData = PopDAT(row, driver, firstAt + 1, firstAt + 1);
            CheckBeat(row, lastData, options, 1, CHI::Eb::Resps::CompData_I, CHI::Eb::RespErrs::NDERR);
            RetireUnacked(row, driver, handle);
            TIMING_CHECK(row, impl.txreq.empty());
        }
    }
}
