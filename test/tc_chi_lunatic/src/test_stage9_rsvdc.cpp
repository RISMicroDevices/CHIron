#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using ConfigRSVDC     = CHI::Eb::FlitConfiguration<7, 48, 4, 8, 256, false, false, false>;
    using ConfigRSVDCWide = CHI::Eb::FlitConfiguration<7, 48, 32, 4, 256, false, false, false>;

    template<typename config>
    using TestDriver = Driver<config>;

    template<typename config>
    using REQ = typename TestDriver<config>::REQ;

    template<typename config>
    using RSP = typename TestDriver<config>::RSP;

    template<typename config>
    using DAT = typename TestDriver<config>::DAT;

    template<typename config>
    Options BackendOptions()
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        return options;
    }

    template<typename config>
    RSP<config> BackendResponse(const REQ<config>& request, uint16_t opcode)
    {
        RSP<config> response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        response.TxnID() = request.TxnID();
        response.DBID() = 77;
        response.Opcode() = opcode;
        response.Resp() = CHI::Eb::Resps::Comp_I;
        response.PCrdType() = request.PCrdType();
        return response;
    }

    template<typename config>
    DAT<config> BackendData(const REQ<config>& request, size_t beat, uint64_t rsvdc,
                            uint16_t opcode = CHI::Eb::Opcodes::DAT::CompData)
    {
        DAT<config> data{};
        data.SrcID() = 6;
        data.TgtID() = 2;
        data.HomeNID() = 2;
        data.TxnID() = request.TxnID();
        data.DBID() = 77;
        data.Opcode() = opcode;
        data.Resp() = CHI::Eb::Resps::CompData_I;
        data.DataID() = beat * (config::dataWidth / 8) / 16;
        data.CCID() = (request.Addr() >> 4) & 3;
        data.RSVDC() = rsvdc;
        data.Data()[0] = 0x0123456789abcdefULL + beat;
        return data;
    }

    template<typename config>
    void SendBackendResponse(TestDriver<config>& driver, const RSP<config>& response)
    {
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    template<typename config>
    void CheckReadWrite(uint64_t reqRsvdc, uint64_t datRsvdc, uint64_t address)
    {
        TestDriver<config> driver(BackendOptions<config>());
        auto read = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, address);
        read.RSVDC() = reqRsvdc;
        LUNATIC_CHECK(read.RSVDC() == reqRsvdc);
        LUNATIC_CHECK(driver.node.PushRXREQ(read));
        REQ<config> downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        LUNATIC_CHECK(downstream.RSVDC() == 0);

        SendBackendResponse(driver, BackendResponse<config>(downstream, CHI::Eb::Opcodes::RSP::RetryAck));
        SendBackendResponse(driver, BackendResponse<config>(downstream, CHI::Eb::Opcodes::RSP::PCrdGrant));
        downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        LUNATIC_CHECK(downstream.RSVDC() == 0);
        LUNATIC_CHECK(!downstream.AllowRetry());
        for (size_t beat = 0; beat < BeatsPerLine(driver.node.GetOptions()); ++beat)
        {
            auto data = BackendData<config>(downstream, beat, datRsvdc);
            LUNATIC_CHECK(data.RSVDC() == datRsvdc);
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
        for (size_t beat = 0; beat < BeatsPerLine(driver.node.GetOptions()); ++beat)
            LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).RSVDC() == 0);
        driver.Quiesce();

        auto write = driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 2, address + 0x1000);
        write.RSVDC() = reqRsvdc;
        LUNATIC_CHECK(driver.node.PushRXREQ(write));
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        for (size_t beat = 0; beat < BeatsPerLine(driver.node.GetOptions()); ++beat)
        {
            DAT<config> data{};
            data.SrcID() = 4;
            data.TgtID() = 2;
            data.TxnID() = dbid;
            data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
            data.DataID() = beat * (config::dataWidth / 8) / 16;
            data.BE() = config::dataWidth == 512 ? ~uint64_t(0) : (uint64_t(1) << (config::dataWidth / 8)) - 1;
            data.RSVDC() = datRsvdc;
            data.Data()[0] = 0xfedcba9876543210ULL + beat;
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
        downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::WriteNoSnpFull);
        LUNATIC_CHECK(downstream.RSVDC() == 0);
        SendBackendResponse(driver, BackendResponse<config>(downstream, CHI::Eb::Opcodes::RSP::DBIDResp));
        for (size_t beat = 0; beat < BeatsPerLine(driver.node.GetOptions()); ++beat)
            LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData).RSVDC() == 0);
        SendBackendResponse(driver, BackendResponse<config>(downstream, CHI::Eb::Opcodes::RSP::Comp));
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void CheckRetry()
    {
        TestDriver<ConfigRSVDCWide> driver;
        driver.node.GetEvents()->OnRequestAdmission.Register(
            Gravity::MakeListener<RequestAdmissionEvent>("rsvdc-retry", 0, [&](RequestAdmissionEvent& event) {
                if (event.request.allowRetry)
                    event.RequestRetry();
            }));
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 3, 0x88000);
        request.RSVDC() = 0xdeadbeef;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        driver.NextRSP(CHI::Eb::Opcodes::RSP::RetryAck);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::PCrdGrant);

        request.TxnID() = 9;
        request.AllowRetry() = 0;
        request.RSVDC() = 0x12345678;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        for (size_t beat = 0; beat < BeatsPerLine(driver.node.GetOptions()); ++beat)
            LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).RSVDC() == 0);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    void CheckAtomic()
    {
        using config = ConfigRSVDC;
        TestDriver<ConfigRSVDC> driver(BackendOptions<ConfigRSVDC>());
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::AtomicSwap, 4, 4, 0x89000);
        request.Size() = 0;
        request.MemAttr() = CHI::Eb::MemAttrs::Device_nEWA;
        request.SnpAttr() = 0;
        request.AllowRetry() = 0;
        request.RSVDC() = 0xf;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        DAT<ConfigRSVDC> operand{};
        operand.SrcID() = 4;
        operand.TgtID() = 2;
        operand.TxnID() = dbid;
        operand.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        operand.BE() = 1;
        operand.Data()[0] = 7;
        operand.RSVDC() = 0xa5;
        LUNATIC_CHECK(driver.node.PushRXDAT(operand));
        driver.Tick();

        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::AtomicSwap);
        LUNATIC_CHECK(downstream.RSVDC() == 0);
        SendBackendResponse(driver, BackendResponse<config>(downstream, CHI::Eb::Opcodes::RSP::DBIDResp));
        LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData).RSVDC() == 0);
        auto result = BackendData<config>(downstream, 0, 0x5a);
        LUNATIC_CHECK(driver.node.PushRXDAT(result));
        driver.Tick();
        LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).RSVDC() == 0);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9RSVDCZeroReadWriteWidths)
{
    CheckReadWrite<ConfigRSVDC>(0xf, 0xa5, 0x8a000);
    CheckReadWrite<ConfigRSVDCWide>(0xdeadbeef, 0xf, 0x8b000);
}

LUNATIC_TEST(Stage9RSVDCZeroRetryResend)
{
    CheckRetry();
}

LUNATIC_TEST(Stage9RSVDCZeroAtomic)
{
    CheckAtomic();
}

LUNATIC_TEST(Stage9RSVDCZeroWidthControl)
{
    TestDriver<Config256> driver;
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 5, 0x8c000);
    static_assert(Config256::reqRsvdcWidth == 0 && Config256::datRsvdcWidth == 0);
    static_assert(std::is_same_v<typename REQ<Config256>::rsvdc_t, std::monostate>);
    static_assert(std::is_same_v<typename DAT<Config256>::rsvdc_t, std::monostate>);
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    for (size_t beat = 0; beat < BeatsPerLine(driver.node.GetOptions()); ++beat)
        driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}
