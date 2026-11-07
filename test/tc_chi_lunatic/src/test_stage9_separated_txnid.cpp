#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename Driver>
    auto StashRequest(Driver& driver, uint16_t form)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::StashOnceSepUnique, 4, 7, 0x1e000);
        request.ReturnNID() = 5;
        request.StashNIDValid() = 1;
        request.StashLPIDValid() = 1;
        request.StashLPID() = 3;
        request.StashGroupID() = 0x32 + form;
        return request;
    }

    void CompleteReplacementRead(CheckedDriver<Config256>& driver, DataBufferId oldDBID)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x1e100));
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TxnID() == 7);
            LUNATIC_CHECK(data.DBID() != oldDBID);
        }
    }

    void RunSeparatedStashTxnIDRelease(uint16_t form)
    {
        CheckedDriver<Config256> driver;
        for (uint16_t index = 0; index < form; ++index)
            driver.ReadData(4, 20 + index, 0x1df00 + index * 0x40, CHI::Eb::Opcodes::REQ::ReadNoSnp);
        driver.PushRequest(StashRequest(driver, form));
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpStashUnique);
        driver.SnoopResponse(5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);

        DataBufferId dbid = 0;
        if (form == 0)
        {
            dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).DBID();
            driver.NextRSP(CHI::Eb::Opcodes::RSP::StashDone);
        }
        else if (form == 1)
        {
            driver.NextRSP(CHI::Eb::Opcodes::RSP::StashDone);
            dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).DBID();
        }
        else
            dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompStashDone).DBID();

        CompleteReplacementRead(driver, dbid);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().admittedTransactions == form + 2);
        LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().retiredTransactions == form + 2);
    }

    template<typename Driver>
    auto StartPersistence(Driver& driver)
    {
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 4, 7, 0x1e200);
        request.PGroupID() = 0x68;
        driver.PushRequest(request);
        return driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
    }

    template<typename Driver>
    auto SendBackendPersistence(Driver& driver, const typename Driver::REQ& request, uint16_t opcode)
    {
        typename Driver::RSP response{};
        response.SrcID() = 6;
        response.TgtID() = 2;
        if (opcode != CHI::Eb::Opcodes::RSP::Persist)
            response.TxnID() = request.TxnID();
        response.Opcode() = opcode;
        response.PGroupID() = 0x68;
        return response;
    }

    void SendCheckedPersistence(CheckedDriver<Config256>& driver, SNOracle<Config256>& oracle,
                                const Driver<Config256>::REQ& request, uint16_t opcode)
    {
        const auto response = SendBackendPersistence(driver, request, opcode);
        oracle.Response(driver.driver.now, response);
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
        driver.driver.Tick();
    }

    void CompleteBackendReplacementRead(CheckedDriver<Config256>& driver, SNOracle<Config256>& oracle,
                                        DataBufferId oldDBID)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x1e100));
        const auto downstream = driver.driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        oracle.Request(driver.driver.now, downstream);
        for (size_t beat = 0; beat < 2; ++beat)
        {
            Driver<Config256>::DAT data{};
            data.SrcID() = 6;
            data.TgtID() = 2;
            data.HomeNID() = 2;
            data.TxnID() = downstream.TxnID();
            data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
            data.Resp() = CHI::Eb::Resps::CompData_I;
            data.DataID() = beat * 2;
            data.CCID() = (downstream.Addr() >> 4) & 3;
            oracle.Data(driver.driver.now, data);
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(data));
        }
        for (size_t beat = 0; beat < 2; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TxnID() == 7);
            LUNATIC_CHECK(data.DBID() != oldDBID);
        }
    }

    void RunSeparatedPersistenceTxnIDRelease(bool persistFirst)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        options.outputQueueLimit = 1;
        CheckedDriver<Config256> driver(options);
        SNOracle<Config256> oracle(2, 6);
        const auto downstream = StartPersistence(driver);
        oracle.Request(driver.driver.now, downstream);
        SendCheckedPersistence(driver, oracle, downstream,
            persistFirst ? CHI::Eb::Opcodes::RSP::Persist : CHI::Eb::Opcodes::RSP::Comp);
        SendCheckedPersistence(driver, oracle, downstream,
            persistFirst ? CHI::Eb::Opcodes::RSP::Comp : CHI::Eb::Opcodes::RSP::Persist);

        const auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Persist);
        CompleteBackendReplacementRead(driver, oracle, comp.DBID());
        driver.Quiesce();
        LUNATIC_CHECK(!oracle.HasInflight());
        LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().admittedTransactions == 2);
        LUNATIC_CHECK(driver.driver.node.GetPerformanceCounters().retiredTransactions == 2);
    }

}

LUNATIC_TEST(Stage9SeparatedTxnIDStashCompThenDone)
{
    RunSeparatedStashTxnIDRelease(0);
}

LUNATIC_TEST(Stage9SeparatedTxnIDStashDoneThenComp)
{
    RunSeparatedStashTxnIDRelease(1);
}

LUNATIC_TEST(Stage9SeparatedTxnIDStashCombined)
{
    RunSeparatedStashTxnIDRelease(2);
}

LUNATIC_TEST(Stage9SeparatedTxnIDStashCompOnlyIsPremature)
{
    Driver<Config256> driver;
    driver.node.PushRXREQ(StashRequest(driver, 0));
    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpStashUnique);
    Driver<Config256>::RSP response{};
    response.SrcID() = 5;
    response.TgtID() = 2;
    response.TxnID() = snoop.flit.TxnID();
    response.Opcode() = CHI::Eb::Opcodes::RSP::SnpResp;
    response.Resp() = CHI::Eb::Resps::SnpResp_I;
    driver.node.PushRXRSP(response);
    driver.Tick();
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);

    driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x1e180));
    driver.Tick();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 1);
    LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    LUNATIC_CHECK(driver.node.GetErrors().front().reason.find("duplicate live requester transaction ID") !=
                  std::string::npos);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::StashDone);
    driver.Quiesce();
}

LUNATIC_TEST(Stage9SeparatedTxnIDPersistenceCompThenPersist)
{
    RunSeparatedPersistenceTxnIDRelease(false);
}

LUNATIC_TEST(Stage9SeparatedTxnIDPersistencePersistThenComp)
{
    RunSeparatedPersistenceTxnIDRelease(true);
}

LUNATIC_TEST(Stage9SeparatedTxnIDPersistenceCompOnlyIsPremature)
{
    Options options;
    options.backend = BackendKind::CHI;
    options.subordinateId = 6;
    options.outputQueueLimit = 1;
    Driver<Config256> driver(options);
    auto request = driver.Request(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep, 4, 7, 0x1e200);
    request.PGroupID() = 0x68;
    driver.node.PushRXREQ(request);
    const auto backend = driver.NextREQ(CHI::Eb::Opcodes::REQ::CleanSharedPersistSep);
    for (const uint16_t opcode : { CHI::Eb::Opcodes::RSP::Comp, CHI::Eb::Opcodes::RSP::Persist })
    {
        const auto response = SendBackendPersistence(driver, backend, opcode);
        driver.node.PushRXRSP(response);
        driver.Tick();
    }

    driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 7, 0x1e280));
    driver.Tick();
    LUNATIC_CHECK(driver.node.GetPerformanceCounters().admittedTransactions == 1);
    LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    LUNATIC_CHECK(driver.node.GetErrors().front().reason.find("duplicate live requester transaction ID") !=
                  std::string::npos);
    driver.NextRSP(CHI::Eb::Opcodes::RSP::Persist);
    driver.Quiesce();
}
