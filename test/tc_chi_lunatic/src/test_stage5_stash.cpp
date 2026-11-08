#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename Driver>
    typename Driver::REQ IndependentRequest(Driver& driver, uint16_t opcode, TransactionId txnId,
                                            uint64_t address, uint8_t group = 0x25)
    {
        auto request = driver.Request(opcode, 4, txnId, address);
        request.ReturnNID() = 5;
        request.StashNIDValid() = 1;
        request.StashLPIDValid() = 1;
        request.StashLPID() = 3;
        if (IsSeparatedStashRequest(opcode))
            request.StashGroupID() = group;
        return request;
    }

    template<typename Snoop>
    void CheckIndependentSnoop(const Snoop& snoop, uint16_t opcode)
    {
        LUNATIC_CHECK(snoop.target == 5);
        LUNATIC_CHECK(snoop.flit.Opcode() == opcode);
        LUNATIC_CHECK(snoop.flit.StashLPIDValid() == 1);
        LUNATIC_CHECK(snoop.flit.StashLPID() == 3);
        LUNATIC_CHECK(snoop.flit.DoNotGoToSD() == 1);
        LUNATIC_CHECK(snoop.flit.RetToSrc() == 0);
    }

    template<typename config>
    void RawSnoopResponse(Driver<config>& driver, NodeId source, TransactionId txnId, uint8_t resp,
                          uint8_t dataPull = 0)
    {
        typename Driver<config>::RSP response{};
        response.SrcID() = source;
        response.TgtID() = 2;
        response.TxnID() = txnId;
        response.Opcode() = CHI::Eb::Opcodes::RSP::SnpResp;
        response.Resp() = resp;
        response.FwdState() = dataPull;
        LUNATIC_CHECK(driver.node.PushRXRSP(response));
        driver.Tick();
    }

    LineData StashPattern()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(31 + index * 7);
        return data;
    }

    ByteEnables StashEnables()
    {
        ByteEnables enables{};
        enables.fill(1);
        return enables;
    }

    void RunNonSeparated(uint16_t opcode)
    {
        Options options;
        options.outputQueueLimit = 1;
        CheckedDriver<Config256> driver(options);
        driver.PushRequest(IndependentRequest(driver, opcode, 9, 0x44000));
        const uint16_t snoopOpcode = opcode == CHI::Eb::Opcodes::REQ::StashOnceUnique
            ? CHI::Eb::Opcodes::SNP::SnpStashUnique : CHI::Eb::Opcodes::SNP::SnpStashShared;
        auto snoop = driver.NextSNP(snoopOpcode);
        CheckIndependentSnoop(snoop, snoopOpcode);
        driver.SnoopResponse(5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 9);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(LineKey::FromAddress(0x44000, false), 5) == CacheState::I);
    }

    void RunSeparatedChecked(uint16_t opcode)
    {
        Options options;
        options.outputQueueLimit = 1;
        CheckedDriver<Config256> driver(options);
        driver.PushRequest(IndependentRequest(driver, opcode, 0, 0x44100, 0x32));
        const uint16_t snoopOpcode = opcode == CHI::Eb::Opcodes::REQ::StashOnceSepUnique
            ? CHI::Eb::Opcodes::SNP::SnpStashUnique : CHI::Eb::Opcodes::SNP::SnpStashShared;
        auto snoop = driver.NextSNP(snoopOpcode);
        CheckIndependentSnoop(snoop, snoopOpcode);
        driver.SnoopResponse(5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 0);
        auto done = driver.NextRSP(CHI::Eb::Opcodes::RSP::StashDone);
        LUNATIC_CHECK(done.TxnID() == 0);
        LUNATIC_CHECK(done.StashGroupID() == 0x32);
        driver.Quiesce();
    }

    void RunSeparatedRaw(uint16_t opcode)
    {
        Options options;
        options.outputQueueLimit = 1;
        Driver<Config256> driver(options);
        const uint16_t snoopOpcode = opcode == CHI::Eb::Opcodes::REQ::StashOnceSepUnique
            ? CHI::Eb::Opcodes::SNP::SnpStashUnique : CHI::Eb::Opcodes::SNP::SnpStashShared;
        for (TransactionHandle form = 0; form < 3; ++form)
        {
            const TransactionId txnId = 0x20 + form;
            const uint8_t group = 0x40 + form;
            auto request = IndependentRequest(driver, opcode, txnId, 0x45000 + form * LINE_SIZE, group);
            LUNATIC_CHECK(driver.node.PushRXREQ(request));
            auto snoop = driver.NextSNP(snoopOpcode);
            CheckIndependentSnoop(snoop, snoopOpcode);
            RawSnoopResponse(driver, 5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
            if (form == 0)
            {
                LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == txnId);
                auto done = driver.NextRSP(CHI::Eb::Opcodes::RSP::StashDone);
                LUNATIC_CHECK(done.TxnID() == 0 && done.StashGroupID() == group);
            }
            else if (form == 1)
            {
                auto done = driver.NextRSP(CHI::Eb::Opcodes::RSP::StashDone);
                LUNATIC_CHECK(done.TxnID() == 0 && done.StashGroupID() == group);
                LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == txnId);
            }
            else
            {
                auto done = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompStashDone);
                LUNATIC_CHECK(done.TxnID() == txnId && done.StashGroupID() == group);
                LUNATIC_CHECK(done.Resp() == CHI::Eb::Resps::Comp_I);
            }
            driver.Quiesce();
            LUNATIC_CHECK(driver.node.GetErrors().empty());
        }
    }

    void RunSharedStatePreservation(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x46000, false);
        const LineData data = StashPattern();
        driver.driver.node.GetMemory().WriteFull(line, data);
        LUNATIC_CHECK(driver.ReadData(5, 1, line.address, CHI::Eb::Opcodes::REQ::ReadShared) == data);
        driver.PushRequest(IndependentRequest(driver, CHI::Eb::Opcodes::REQ::StashOnceShared, 2, line.address));
        auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpStashShared);
        driver.SnoopResponse(5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_SC);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 2);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::SC);
    }

    void RunDirtyStatePreservation(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(0x46100, false);
        const LineData data = StashPattern();
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, 1, line.address));
        auto write = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 1, write.DBID(), data, StashEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
                         CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(driver.ReadData(5, 2, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::UD);

        driver.PushRequest(IndependentRequest(driver, CHI::Eb::Opcodes::REQ::StashOnceUnique, 3, line.address));
        auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpStashUnique);
        driver.SnoopResponse(5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_UD);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 3);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::UD);
    }

}

LUNATIC_TEST(Stage5IndependentStashComp)
{
    RunNonSeparated(CHI::Eb::Opcodes::REQ::StashOnceUnique);
    RunNonSeparated(CHI::Eb::Opcodes::REQ::StashOnceShared);
}

LUNATIC_TEST(Stage5SeparatedStashCHIronResponses)
{
    RunSeparatedChecked(CHI::Eb::Opcodes::REQ::StashOnceSepUnique);
    RunSeparatedChecked(CHI::Eb::Opcodes::REQ::StashOnceSepShared);
}

LUNATIC_TEST(Stage5SeparatedStashResponseFormsAndBackpressure)
{
    RunSeparatedRaw(CHI::Eb::Opcodes::REQ::StashOnceSepUnique);
    RunSeparatedRaw(CHI::Eb::Opcodes::REQ::StashOnceSepShared);
}

LUNATIC_TEST(Stage5IndependentStashPreservesTargetState)
{
    RunSharedStatePreservation(PolicyKind::NINE);
    RunSharedStatePreservation(PolicyKind::Exclusive);
    RunSharedStatePreservation(PolicyKind::Inclusive);
    RunDirtyStatePreservation(PolicyKind::NINE);
    RunDirtyStatePreservation(PolicyKind::Exclusive);
    RunDirtyStatePreservation(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5IndependentStashReportsDataPull)
{
    Driver<Config256> driver;
    auto request = IndependentRequest(driver, CHI::Eb::Opcodes::REQ::StashOnceUnique, 7, 0x47000);
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpStashUnique);
    RawSnoopResponse(driver, 5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I, 1);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 7);
    DataBufferId impliedDBID = 0;
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.TgtID() == 5 && data.TxnID() == 0);
        LUNATIC_CHECK(data.RespErr() == CHI::Eb::RespErrs::OK);
        impliedDBID = data.DBID();
    }
    driver.CompAck(5, impliedDBID);
    driver.Quiesce();
    LUNATIC_CHECK(driver.node.GetErrors().empty());
}

LUNATIC_TEST(Stage5IndependentStashRejectsInvalidGeometry)
{
    Driver<Config256> driver;
    auto request = IndependentRequest(driver, CHI::Eb::Opcodes::REQ::StashOnceUnique, 8, 0x47100);
    request.Size() = 4;
    LUNATIC_CHECK(driver.node.PushRXREQ(request));
    auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
    LUNATIC_CHECK(comp.TxnID() == 8 && comp.RespErr() == 0b10);
    driver.Quiesce();
    LUNATIC_CHECK(!driver.node.GetErrors().empty());
}

namespace {

    template<typename Driver>
    typename Driver::REQ WriteStashRequest(Driver& driver, uint16_t opcode, TransactionId txnId,
                                           uint64_t address, bool compAck = false)
    {
        auto request = driver.Request(opcode, 4, txnId, address, compAck);
        if (compAck)
            request.Order() = 2;
        request.ReturnNID() = 5;
        request.StashNIDValid() = 1;
        request.StashLPIDValid() = 1;
        request.StashLPID() = 3;
        return request;
    }

    template<typename config>
    void SeedHome(CheckedDriver<config>& driver, const LineKey& line, const LineData& data, TransactionId txnId)
    {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 6, txnId, line.address));
        auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, txnId, response.DBID(), data, StashEnables(), CHI::Eb::Opcodes::DAT::CopyBackWrData,
                         CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
    }

    template<typename config>
    void MakeDirtyTarget(CheckedDriver<config>& driver, const LineKey& line, const LineData& data)
    {
        SeedHome(driver, line, data, 90);
        LUNATIC_CHECK(driver.ReadData(5, 91, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::UD);
    }

    template<typename config>
    void CompleteWriteStash(CheckedDriver<config>& driver, TransactionId txnId, const LineData& data,
                            const ByteEnables& enables, uint16_t dataOpcode = CHI::Eb::Opcodes::DAT::NonCopyBackWrData)
    {
        driver.driver.Until([&] { return static_cast<bool>(driver.driver.node.PeekTXRSP()); });
        const uint16_t firstOpcode = driver.driver.node.PeekTXRSP()->Opcode();
        if (firstOpcode == CHI::Eb::Opcodes::RSP::Comp)
        {
            LUNATIC_CHECK(driver.NextRSP(firstOpcode).TxnID() == txnId);
            const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
            driver.WriteData(4, txnId, dbid, data, enables, dataOpcode);
        }
        else if (firstOpcode == CHI::Eb::Opcodes::RSP::DBIDResp)
        {
            const DataBufferId dbid = driver.NextRSP(firstOpcode).DBID();
            driver.WriteData(4, txnId, dbid, data, enables, dataOpcode);
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == txnId);
        }
        else
        {
            LUNATIC_CHECK(firstOpcode == CHI::Eb::Opcodes::RSP::CompDBIDResp);
            const DataBufferId dbid = driver.NextRSP(firstOpcode).DBID();
            driver.WriteData(4, txnId, dbid, data, enables, dataOpcode);
        }
    }

    template<typename config>
    void RunWriteStashMatrix(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        options.outputQueueLimit = 1;
        CheckedDriver<config> driver(options);
        const LineData initial = StashPattern();
        LineData update{};
        for (size_t index = 0; index < update.size(); ++index)
            update[index] = initial[index] ^ 0xa5;

        for (size_t iteration = 0; iteration < 6; ++iteration)
        {
            const bool full = iteration % 2 != 0;
            const LineKey line = LineKey::FromAddress(0x48000 + iteration * LINE_SIZE, false);
            SeedHome(driver, line, initial, 100 + iteration * 2);
            const uint16_t opcode = full ? CHI::Eb::Opcodes::REQ::WriteUniqueFullStash
                : CHI::Eb::Opcodes::REQ::WriteUniquePtlStash;
            const TransactionId txnId = 101 + iteration * 2;
            driver.PushRequest(WriteStashRequest(driver, opcode, txnId, line.address));
            const uint16_t snoopOpcode = full ? CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash
                : CHI::Eb::Opcodes::SNP::SnpUniqueStash;
            auto snoop = driver.NextSNP(snoopOpcode);
            CheckIndependentSnoop(snoop, snoopOpcode);
            driver.SnoopResponse(5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
            ByteEnables enables{};
            for (size_t index = 0; index < enables.size(); ++index)
                enables[index] = full || (index >= 7 && index < 51);
            CompleteWriteStash(driver, txnId, update, enables);
            driver.Quiesce();

            LineData expected = initial;
            for (size_t index = 0; index < expected.size(); ++index)
                if (enables[index])
                    expected[index] = update[index];
            LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == expected);
            LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::I);
            LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::I);
        }
    }

    void RunDirtyWriteStash(PolicyKind policy, bool full)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<Config256> driver(options);
        const LineKey line = LineKey::FromAddress(full ? 0x49100 : 0x49000, false);
        const LineData initial = StashPattern();
        LineData update{};
        for (size_t index = 0; index < update.size(); ++index)
            update[index] = initial[index] ^ 0x5a;
        MakeDirtyTarget(driver, line, initial);

        const uint16_t opcode = full ? CHI::Eb::Opcodes::REQ::WriteUniqueFullStash
            : CHI::Eb::Opcodes::REQ::WriteUniquePtlStash;
        driver.PushRequest(WriteStashRequest(driver, opcode, 7, line.address));
        const uint16_t snoopOpcode = full ? CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash
            : CHI::Eb::Opcodes::SNP::SnpUniqueStash;
        auto snoop = driver.NextSNP(snoopOpcode);
        CheckIndependentSnoop(snoop, snoopOpcode);
        if (full)
            driver.SnoopResponse(5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        else
            driver.SnoopData(5, snoop.flit.TxnID(), initial, CHI::Eb::Resps::SnpRespData_I_PD);
        ByteEnables enables{};
        for (size_t index = 0; index < enables.size(); ++index)
            enables[index] = full || (index >= 11 && index < 47);
        CompleteWriteStash(driver, 7, update, enables);
        driver.Quiesce();

        LineData expected = initial;
        for (size_t index = 0; index < expected.size(); ++index)
            if (enables[index])
                expected[index] = update[index];
        LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == expected);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::I);
    }

}

LUNATIC_TEST(Stage5WriteStashMatrix128)
{
    RunWriteStashMatrix<Config128>(PolicyKind::NINE);
    RunWriteStashMatrix<Config128>(PolicyKind::Exclusive);
    RunWriteStashMatrix<Config128>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5WriteStashMatrix256)
{
    RunWriteStashMatrix<Config256>(PolicyKind::NINE);
    RunWriteStashMatrix<Config256>(PolicyKind::Exclusive);
    RunWriteStashMatrix<Config256>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5WriteStashMatrix512)
{
    RunWriteStashMatrix<Config512>(PolicyKind::NINE);
    RunWriteStashMatrix<Config512>(PolicyKind::Exclusive);
    RunWriteStashMatrix<Config512>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5WriteStashDirtyTargets)
{
    RunDirtyWriteStash(PolicyKind::NINE, false);
    RunDirtyWriteStash(PolicyKind::NINE, true);
    RunDirtyWriteStash(PolicyKind::Exclusive, false);
    RunDirtyWriteStash(PolicyKind::Exclusive, true);
    RunDirtyWriteStash(PolicyKind::Inclusive, false);
    RunDirtyWriteStash(PolicyKind::Inclusive, true);
}

LUNATIC_TEST(Stage5WriteStashAcceptsPostDBIDEarlyCompAck)
{
    CheckedDriver<Config256> driver;
    const LineKey line = LineKey::FromAddress(0x49200, false);
    const LineData data = StashPattern();
    driver.PushRequest(WriteStashRequest(driver, CHI::Eb::Opcodes::REQ::WriteUniqueFullStash, 1, line.address, true));
    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash);
    driver.SnoopResponse(5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
    const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDRespOrd).DBID();
    driver.CompAck(4, dbid);
    driver.WriteData(4, 1, dbid, data, StashEnables(), CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 1);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectHome(line).data == data);
}

LUNATIC_TEST(Stage5WriteStashRejectsUnexpectedCombinedAckAndBadGeometry)
{
    {
        Driver<Config256> driver;
        auto request = WriteStashRequest(driver, CHI::Eb::Opcodes::REQ::WriteUniquePtlStash, 3, 0x49300);
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUniqueStash);
        RawSnoopResponse(driver, 5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
        driver.WriteData(4, 3, dbid, StashPattern(), StashEnables(), CHI::Eb::Opcodes::DAT::NCBWrDataCompAck);
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
    }
    {
        Driver<Config256> driver;
        auto request = WriteStashRequest(driver, CHI::Eb::Opcodes::REQ::WriteUniqueFullStash, 4, 0x49400);
        request.Size() = 4;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        LUNATIC_CHECK(comp.TxnID() == 4 && comp.RespErr() == 0b10);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
    }
}
