#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    void RunInternalSeparatedRead(uint16_t opcode, uint8_t order, bool expCompAck, size_t ackAfterBeats)
    {
        Options options;
        options.separatedReadResponse = true;
        CheckedDriver<config> driver(options);
        auto request = driver.Request(opcode, 4, 1, 0x10000, expCompAck);
        request.Order() = order;
        if (opcode != CHI::Eb::Opcodes::REQ::ReadNoSnp)
        {
            request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
            request.SnpAttr() = 1;
        }
        driver.PushRequest(request);

        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
        LUNATIC_CHECK(response.Resp() == 0);
        const DataBufferId dataBufferId = response.DBID();
        constexpr size_t beats = LINE_SIZE / (config::dataWidth / 8);
        if (ackAfterBeats == 0)
            driver.CompAck(4, dataBufferId);
        for (size_t beat = 0; beat < beats; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::DataSepResp);
            LUNATIC_CHECK(data.Resp() == CHI::Eb::Resps::DataSepResp_UC);
            LUNATIC_CHECK(data.DataID() == beat * (config::dataWidth / 8) / 16);
            if (beat + 1 == ackAfterBeats)
                driver.CompAck(4, dataBufferId);
        }
        driver.Quiesce();
    }

    template<typename config>
    void RunOrderedFallback()
    {
        Options options;
        options.separatedReadResponse = true;
        options.separatedBackendRead = true;
        CheckedDriver<config> driver(options);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, false);
        request.Order() = 2;
        driver.PushRequest(request);

        driver.NextRSP(CHI::Eb::Opcodes::RSP::ReadReceipt);
        constexpr size_t beats = LINE_SIZE / (config::dataWidth / 8);
        for (size_t beat = 0; beat < beats; ++beat)
            driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        driver.Quiesce();
    }

    template<typename config>
    void SendBackendRead(CheckedDriver<config>& driver, SNOracle<config>& oracle,
                         const typename CheckedDriver<config>::REQ& request)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        const bool separated = request.Opcode() == CHI::Eb::Opcodes::REQ::ReadNoSnpSep;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            typename CheckedDriver<config>::DAT response{};
            response.SrcID() = 6;
            response.TgtID() = 2;
            response.HomeNID() = 2;
            response.TxnID() = request.TxnID();
            response.Opcode() = separated ? CHI::Eb::Opcodes::DAT::DataSepResp : CHI::Eb::Opcodes::DAT::CompData;
            response.Resp() = separated ? CHI::Eb::Resps::DataSepResp_UC : CHI::Eb::Resps::CompData_I;
            response.DataID() = beat * bytes / 16;
            response.CCID() = (request.Addr() >> 4) & 3;
            oracle.Data(driver.driver.now, response);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(response));
        }
        driver.driver.Tick();
    }

    template<typename config>
    void RunIndependentBackendPolicy(bool upstreamSeparated, bool backendSeparated)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.separatedReadResponse = upstreamSeparated;
        options.separatedBackendRead = backendSeparated;
        CheckedDriver<config> driver(options);
        SNOracle<config> oracle(2, 6);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, false);
        driver.PushRequest(request);

        const auto downstream = driver.driver.NextREQ(backendSeparated
            ? CHI::Eb::Opcodes::REQ::ReadNoSnpSep : CHI::Eb::Opcodes::REQ::ReadNoSnp);
        oracle.Request(driver.driver.now, downstream);
        SendBackendRead(driver, oracle, downstream);

        DataBufferId dataBufferId = 0;
        if (upstreamSeparated)
        {
            const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
            dataBufferId = response.DBID();
        }
        constexpr size_t beats = LINE_SIZE / (config::dataWidth / 8);
        for (size_t beat = 0; beat < beats; ++beat)
        {
            const auto data = driver.NextDAT(upstreamSeparated
                ? CHI::Eb::Opcodes::DAT::DataSepResp : CHI::Eb::Opcodes::DAT::CompData);
            dataBufferId = data.DBID();
        }
        (void)dataBufferId;
        driver.Quiesce();
        LUNATIC_CHECK(!oracle.HasInflight());
    }

}

LUNATIC_TEST(Stage6SeparatedReadRejectsRNReadNoSnpSep)
{
    LUNATIC_CHECK(ClassifyRequest(CHI::Eb::Opcodes::REQ::ReadNoSnpSep) == RequestClass::Unsupported);
    LUNATIC_CHECK(!IsSupportedRequest(CHI::Eb::Opcodes::REQ::ReadNoSnpSep));
}

LUNATIC_TEST(Stage6SeparatedReadUnorderedEarlyCompAck128)
{
    RunInternalSeparatedRead<Config128>(CHI::Eb::Opcodes::REQ::ReadNoSnp, 0, true, 0);
}

LUNATIC_TEST(Stage6SeparatedReadUnorderedEarlyCompAck256)
{
    RunInternalSeparatedRead<Config256>(CHI::Eb::Opcodes::REQ::ReadNoSnp, 0, true, 0);
}

LUNATIC_TEST(Stage6SeparatedReadUnorderedEarlyCompAck512)
{
    RunInternalSeparatedRead<Config512>(CHI::Eb::Opcodes::REQ::ReadNoSnp, 0, true, 0);
}

LUNATIC_TEST(Stage6SeparatedReadOrderedCompAckAfterFirstData128)
{
    RunInternalSeparatedRead<Config128>(CHI::Eb::Opcodes::REQ::ReadOnce, 2, true, 1);
}

LUNATIC_TEST(Stage6SeparatedReadOrderedCompAckAfterFirstData256)
{
    RunInternalSeparatedRead<Config256>(CHI::Eb::Opcodes::REQ::ReadOnceCleanInvalid, 2, true, 1);
}

LUNATIC_TEST(Stage6SeparatedReadOrderedCompAckAfterFirstData512)
{
    RunInternalSeparatedRead<Config512>(CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid, 2, true, 1);
}

LUNATIC_TEST(Stage6SeparatedReadOrderedNoCompAckFallsBack)
{
    RunOrderedFallback<Config256>();
}

LUNATIC_TEST(Stage6SeparatedReadUnorderedNoCompAck)
{
    Options options;
    options.separatedReadResponse = true;
    CheckedDriver<Config256> driver(options);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, false);
    driver.PushRequest(request);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::DataSepResp);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::DataSepResp);
    driver.Quiesce();
}

LUNATIC_TEST(Stage6SeparatedReadRejectsOrderedCompAckBeforeData)
{
    Options options;
    options.separatedReadResponse = true;
    Driver<Config256> driver(options);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, true);
    request.Order() = 2;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
    driver.CompAck(4, response.DBID());
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage6SeparatedReadBackpressureAndPeekPurity)
{
    Options options;
    options.outputQueueLimit = 1;
    options.separatedReadResponse = true;
    Driver<Config256> driver(options);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x10000, false);
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    driver.Until([&] { return static_cast<bool>(driver.node.PeekTXRSP()); });

    const auto first = driver.node.PeekTXRSP();
    const auto second = driver.node.PeekTXRSP();
    LUNATIC_CHECK(first && second);
    LUNATIC_CHECK(first->Opcode() == CHI::Eb::Opcodes::RSP::RespSepData);
    LUNATIC_CHECK(second->Opcode() == first->Opcode());
    LUNATIC_CHECK(second->TxnID() == first->TxnID());
    LUNATIC_CHECK(second->DBID() == first->DBID());
    const auto pendingData = driver.node.PeekTXDAT();
    LUNATIC_CHECK(pendingData && pendingData->Opcode() == CHI::Eb::Opcodes::DAT::DataSepResp);

    const auto response = driver.node.PopTXRSP();
    LUNATIC_CHECK(response && response->Opcode() == CHI::Eb::Opcodes::RSP::RespSepData);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::DataSepResp);
    driver.NextDAT(CHI::Eb::Opcodes::DAT::DataSepResp);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage6SeparatedReadUpstreamOnlyPolicy)
{
    RunIndependentBackendPolicy<Config256>(true, false);
}

LUNATIC_TEST(Stage6SeparatedReadDownstreamOnlyPolicy)
{
    RunIndependentBackendPolicy<Config256>(false, true);
}

LUNATIC_TEST(Stage6SeparatedReadIndependentPolicies128)
{
    RunIndependentBackendPolicy<Config128>(true, false);
}

LUNATIC_TEST(Stage6SeparatedReadIndependentPolicies512)
{
    RunIndependentBackendPolicy<Config512>(false, true);
}
