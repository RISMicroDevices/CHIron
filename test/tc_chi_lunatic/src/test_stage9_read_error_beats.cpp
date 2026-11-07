#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using TestDriver = Driver<Config256>;
    using REQ        = TestDriver::REQ;
    using DAT        = TestDriver::DAT;

    REQ StartRead(TestDriver& driver, uint64_t address)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, address)));
        return driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
    }

    void SendReadData(TestDriver& driver, const REQ& request, const uint8_t (&errors)[2])
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 2;
            data.HomeNID() = 2;
            data.TxnID() = request.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = CHI::Eb::Resps::CompData_I;
            data.RespErr() = errors[beat];
            data.DataID() = beat * 2;
            data.CCID() = (request.Addr() >> 4) & 3;
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
    }

    void CheckReadErrors(bool separated, const uint8_t (&errors)[2], uint64_t address)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.separatedReadResponse = separated;
        TestDriver driver(options);
        const auto downstream = StartRead(driver, address);
        SendReadData(driver, downstream, errors);

        if (separated)
        {
            const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
            LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::OK);
        }
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(separated ? CHI::Eb::Opcodes::DAT::DataSepResp
                                                       : CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.DataID() == beat * 2);
            LUNATIC_CHECK(data.RespErr() == errors[beat]);
        }
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9ReadDataErrorsPreserveEveryBeat)
{
    const uint8_t first[] = { CHI::Eb::RespErrs::DERR, CHI::Eb::RespErrs::OK };
    const uint8_t second[] = { CHI::Eb::RespErrs::OK, CHI::Eb::RespErrs::DERR };
    CheckReadErrors(false, first, 0x69200);
    CheckReadErrors(false, second, 0x69300);
}

LUNATIC_TEST(Stage9SeparatedReadDataErrorsKeepResponseOK)
{
    const uint8_t errors[] = { CHI::Eb::RespErrs::DERR, CHI::Eb::RespErrs::DERR };
    CheckReadErrors(true, errors, 0x69400);
}
