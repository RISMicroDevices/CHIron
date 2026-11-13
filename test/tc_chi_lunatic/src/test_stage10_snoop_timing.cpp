#include "common.hpp"
#include "chi/icn/lunatic/chi_lunatic_core.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    class Driver {
    public:
        Core::Impl impl;

        explicit Driver(const Options& options)
            : impl(2, options, std::make_shared<EventHub>())
        {
        }

        Transaction& AddRead()
        {
            Request request;
            request.source = 4;
            request.target = 2;
            request.txnId = 1;
            request.opcode = CHI::Eb::Opcodes::REQ::ReadUnique;
            request.address = 0x1000;
            request.size = 6;
            request.allowRetry = false;
            request.expCompAck = true;
            request.snpAttr = true;
            request.memAttr = CHI::Eb::MemAttrs::WriteBack_Allocate;
            impl.time = 1;
            impl.AdmitRequest(request);
            Transaction* transaction = impl.tracker.FindRequest(4, 1);
            LUNATIC_CHECK(transaction != nullptr);
            transaction->dbid = transaction->handle;
            LUNATIC_CHECK(impl.tracker.BindDBID(transaction->handle, transaction->handle));
            return *transaction;
        }

        void SeedHome(Transaction& transaction, bool peer)
        {
            LineData data{};
            for (size_t index = 0; index < data.size(); ++index)
                data[index] = uint8_t(0x40 + index);
            const LineKey line = transaction.request.Line();
            impl.InstallHome(line, 0, data, true, impl.time);
            transaction.homeHit = true;
            transaction.data = data;
            FlowState& flow = impl.flows.at(transaction.handle);
            flow.homeDirty = true;
            flow.validBytes.fill(1);
            flow.snoopLine = line;
            if (peer)
                impl.Bank(line).snoopFilter.Track(line, 5, CacheState::UD, impl.time);
        }

        void TickTo(Time target)
        {
            while (impl.time < target)
            {
                ++impl.time;
                impl.Tick();
            }
        }

        Snoop PopSnoop(Time time)
        {
            TickTo(time);
            const auto snoop = impl.Pop(impl.txsnp, EventKind::EmittedTXSNP);
            LUNATIC_CHECK(snoop.has_value());
            return *snoop;
        }

        Data PopData(Time time, uint8_t dataId)
        {
            TickTo(time);
            const auto data = impl.Pop(impl.txdat, EventKind::EmittedTXDAT);
            LUNATIC_CHECK(data && data->opcode == CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data->dataId == dataId);
            for (size_t index = 0; index < BytesPerBeat(impl.options); ++index)
                LUNATIC_CHECK(data->bytes[index] == uint8_t(0x40 + data->ByteOffset() + index));
            return *data;
        }

        void SendSnoopData(Transaction& transaction, const Snoop& snoop, Time firstBeat)
        {
            for (size_t beat = 0; beat < BeatsPerLine(impl.options); ++beat)
            {
                Data data;
                data.source = snoop.target;
                data.target = 2;
                data.homeNid = 2;
                data.txnId = snoop.txnId;
                data.opcode = CHI::Eb::Opcodes::DAT::SnpRespData;
                data.resp = CHI::Eb::Resps::SnpRespData_I_PD;
                data.dataId = beat * BytesPerBeat(impl.options) / 16;
                data.ccid = 0;
                for (size_t index = 0; index < BytesPerBeat(impl.options); ++index)
                {
                    data.bytes[index] = transaction.data[beat * BytesPerBeat(impl.options) + index];
                    data.byteEnables[index] = 1;
                }
                impl.time = firstBeat + beat;
                impl.ProcessSnoopDAT(transaction, data);
            }
        }

        void Finish(const Transaction& transaction)
        {
            const TransactionHandle handle = transaction.handle;
            LUNATIC_CHECK(transaction.phase == Phase::WaitCompAck);
            Response response;
            response.source = transaction.request.source;
            response.target = 2;
            response.txnId = transaction.dbid;
            response.opcode = CHI::Eb::Opcodes::RSP::CompAck;
            impl.ProcessRSP(response);
            LUNATIC_CHECK(transaction.phase == Phase::Commit);
            impl.RetireFinished();
            LUNATIC_CHECK(impl.tracker.Find(handle) == nullptr);
            LUNATIC_CHECK(impl.errors.empty());
            LUNATIC_CHECK(impl.ValidateInvariants());
            LUNATIC_CHECK(!impl.HasInflight());
        }
    };

    Options TestOptions()
    {
        Options options = MakeType3Options();
        options.backend = BackendKind::Internal;
        options.poisonEnabled = false;
        options.dataCheckEnabled = false;
        return options;
    }

    void CheckHomeHitLatency(uint64_t homeHitLatency, uint64_t dataLatency, Time expected)
    {
        Options options = TestOptions();
        options.homeHitDataLatency = homeHitLatency;
        options.dataLatency = dataLatency;
        Driver driver(options);
        Transaction& transaction = driver.AddRead();
        driver.SeedHome(transaction, false);

        driver.impl.PlanReadSnoops(transaction);
        LUNATIC_CHECK(transaction.phase == Phase::Action);
        LUNATIC_CHECK(transaction.readyTime == expected);
        LUNATIC_CHECK(driver.impl.txsnp.empty());
        LUNATIC_CHECK(driver.impl.txdat.empty());

        driver.PopData(expected + 1, 0);
        LUNATIC_CHECK(transaction.phase == Phase::Return);
        driver.PopData(expected + 2, 2);
        driver.Finish(transaction);
    }

}

void CheckSnoopDataPortTiming(uint64_t snoopDataLatency, Time expected)
{
    Options options = TestOptions();
    options.snoopDataLatency = snoopDataLatency;
    Driver driver(options);
    Transaction& transaction = driver.AddRead();
    driver.SeedHome(transaction, true);

    driver.impl.PlanReadSnoops(transaction);
    LUNATIC_CHECK(transaction.phase == Phase::WaitSnoop);
    LUNATIC_CHECK(transaction.pendingSnoops.size() == 1);
    LUNATIC_CHECK(driver.impl.txsnp.size() == 1);

    driver.impl.ActionCoherent(transaction);
    LUNATIC_CHECK(driver.impl.txsnp.size() == 1);

    const Snoop snoop = driver.PopSnoop(1);
    LUNATIC_CHECK(snoop.opcode == CHI::Eb::Opcodes::SNP::SnpUnique);
    driver.SendSnoopData(transaction, snoop, 2);
    LUNATIC_CHECK(transaction.phase == Phase::Return);
    LUNATIC_CHECK(transaction.wait == WaitReason::None);
    LUNATIC_CHECK(transaction.readyTime == expected);
    LUNATIC_CHECK(driver.impl.txdat.empty());

    driver.PopData(expected, 0);
    driver.PopData(expected + 1, 2);
    driver.Finish(transaction);
}

LUNATIC_TEST(Stage10SnoopResolveAndDataPortTiming)
{
    CheckSnoopDataPortTiming(4, 7);
    CheckSnoopDataPortTiming(0, 6);
}

LUNATIC_TEST(Stage10HomeHitDataLatencyOverride)
{
    CheckHomeHitLatency(2, 3, 3);
    CheckHomeHitLatency(0, 4, 5);
}
