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

        Transaction& Add(uint16_t opcode)
        {
            Request request;
            request.source = 4;
            request.target = 2;
            request.txnId = 1;
            request.opcode = opcode;
            request.address = 0x1000;
            request.size = 6;
            request.allowRetry = true;
            request.snpAttr = true;
            request.memAttr = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
            impl.time = 1;
            impl.AdmitRequest(request);
            Transaction* transaction = impl.tracker.FindRequest(4, 1);
            LUNATIC_CHECK(transaction != nullptr);
            transaction->dbid = transaction->handle;
            return *transaction;
        }

        void TickTo(Time time)
        {
            while (impl.time < time)
            {
                ++impl.time;
                impl.Tick();
            }
        }

        void PopResponse(Time time, uint16_t opcode)
        {
            TickTo(time);
            const auto response = impl.Pop(impl.txrsp, EventKind::EmittedTXRSP);
            LUNATIC_CHECK(response && response->opcode == opcode);
        }

        Request PopRequest(Time time, uint16_t opcode)
        {
            TickTo(time);
            const auto request = impl.Pop(impl.txreq, EventKind::EmittedTXREQ);
            LUNATIC_CHECK(request && request->opcode == opcode);
            return *request;
        }

        void PopData(Time time, uint8_t dataId)
        {
            TickTo(time);
            const auto data = impl.Pop(impl.txdat, EventKind::EmittedTXDAT);
            LUNATIC_CHECK(data && data->dataId == dataId);
        }

        void SendWriteData(Transaction& transaction)
        {
            for (size_t beat = 0; beat < 2; ++beat)
            {
                Data data;
                data.source = 4;
                data.target = 2;
                data.homeNid = 2;
                data.txnId = transaction.dbid;
                data.opcode = CHI::Eb::Opcodes::DAT::CopyBackWrData;
                data.resp = CHI::Eb::Resps::CopyBackWrData_UD_PD;
                data.dataId = beat * 2;
                data.ccid = 0;
                for (size_t index = 0; index < 32; ++index)
                {
                    data.bytes[index] = uint8_t(0xe0 + beat * 32 + index);
                    data.byteEnables[index] = 1;
                }
                LUNATIC_CHECK(impl.HandleWriteDAT(transaction, data));
            }
        }

        void CheckClean()
        {
            LUNATIC_CHECK(impl.errors.empty());
            LUNATIC_CHECK(impl.ValidateInvariants());
            LUNATIC_CHECK(!impl.HasInflight());
        }
    };

}

LUNATIC_TEST(Stage10CopybackResolveResponsePipeline)
{
    Options options = MakeType3Options();
    options.subordinateId = 6;
    Driver driver(options);
    Transaction& transaction = driver.Add(CHI::Eb::Opcodes::REQ::WriteCleanFull);
    const TransactionHandle handle = transaction.handle;

    driver.PopResponse(7, CHI::Eb::Opcodes::RSP::CompDBIDResp);
    LUNATIC_CHECK(transaction.phase == Phase::WaitWriteData);
    driver.impl.time = 8;
    driver.SendWriteData(transaction);
    LUNATIC_CHECK(transaction.readyTime == 11);

    const Request backend = driver.PopRequest(11, CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
    Response response;
    response.source = 6;
    response.target = 2;
    response.txnId = backend.txnId;
    response.opcode = CHI::Eb::Opcodes::RSP::CompDBIDResp;
    response.dbid = 0xa5;
    LUNATIC_CHECK(driver.impl.HandleBackendRSP(transaction, response));

    driver.impl.ActionCoherent(transaction);
    LUNATIC_CHECK(driver.impl.txdat.front().readyTime == 15);
    driver.PopData(15, 0);
    driver.impl.AdvanceBackendWrite(transaction);
    driver.PopData(16, 2);
    driver.impl.ActionCoherent(transaction);
    driver.impl.RetireFinished();
    driver.TickTo(17);
    LUNATIC_CHECK(driver.impl.tracker.Find(handle) == nullptr);
    driver.CheckClean();

    Options legacy;
    Driver legacyDriver(legacy);
    legacyDriver.Add(CHI::Eb::Opcodes::REQ::WriteCleanFull);
    legacyDriver.TickTo(7);
    LUNATIC_CHECK(legacyDriver.impl.Peek(legacyDriver.impl.txrsp) == nullptr);
    legacyDriver.PopResponse(10, CHI::Eb::Opcodes::RSP::CompDBIDResp);
}
