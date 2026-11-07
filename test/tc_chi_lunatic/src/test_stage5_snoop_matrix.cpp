#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    using MatrixDriver = CheckedDriver<Config256>;

    LineData MatrixPattern()
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(37 + index * 11);
        return data;
    }

    ByteEnables MatrixEnables(bool partial = false)
    {
        ByteEnables enables{};
        for (size_t index = 0; index < enables.size(); ++index)
            enables[index] = !partial || (index >= 9 && index < 47);
        return enables;
    }

    uint8_t PreciseStashResponse(CacheState state)
    {
        switch (state)
        {
            case CacheState::I: return CHI::Eb::Resps::SnpResp_I;
            case CacheState::UC:
            case CacheState::UCE: return CHI::Eb::Resps::SnpResp_UC;
            case CacheState::UD:
            case CacheState::UDP: return CHI::Eb::Resps::SnpResp_UD;
            case CacheState::SC: return CHI::Eb::Resps::SnpResp_SC;
            case CacheState::SD: return CHI::Eb::Resps::SnpResp_SD;
        }
        return CHI::Eb::Resps::SnpResp_I;
    }

    LineData ReceiveMatrixData(MatrixDriver& driver, NodeId source, TransactionId txnId)
    {
        LineData result{};
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < 2; ++beat)
        {
            auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.TgtID() == source && data.TxnID() == txnId);
            dbid = data.DBID();
            const size_t offset = size_t(data.DataID()) * 16;
            for (size_t index = 0; index < 32; ++index)
                result[offset + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
        }
        driver.CompAck(source, dbid);
        return result;
    }

    void SeedMatrixHome(MatrixDriver& driver, const LineKey& line, bool partial = false)
    {
        const LineData data = MatrixPattern();
        const uint16_t opcode = partial ? CHI::Eb::Opcodes::REQ::WriteBackPtl
            : CHI::Eb::Opcodes::REQ::WriteBackFull;
        driver.PushRequest(driver.Request(opcode, 6, 10, line.address));
        auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(6, 10, response.DBID(), data, MatrixEnables(partial),
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
    }

    void SeedMatrixTarget(MatrixDriver& driver, const LineKey& line, CacheState state)
    {
        const LineData data = MatrixPattern();
        driver.driver.node.GetMemory().WriteFull(line, data);
        switch (state)
        {
            case CacheState::I:
                break;
            case CacheState::UC:
                LUNATIC_CHECK(driver.ReadData(5, 11, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
                break;
            case CacheState::UCE:
            {
                driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::CleanUnique, 5, 11, line.address, true));
                auto comp = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
                driver.CompAck(5, comp.DBID());
                break;
            }
            case CacheState::SC:
                LUNATIC_CHECK(driver.ReadData(5, 11, line.address, CHI::Eb::Opcodes::REQ::ReadShared) == data);
                break;
            case CacheState::UD:
                SeedMatrixHome(driver, line);
                LUNATIC_CHECK(driver.ReadData(5, 11, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
                break;
            case CacheState::UDP:
                SeedMatrixHome(driver, line, true);
                LUNATIC_CHECK(driver.ReadData(5, 11, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
                break;
            case CacheState::SD:
                SeedMatrixHome(driver, line);
                LUNATIC_CHECK(driver.ReadData(6, 11, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == data);
                driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 5, 12, line.address, true));
                {
                    auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
                    driver.SnoopData(6, snoop.flit.TxnID(), data, CHI::Eb::Resps::SnpRespData_SC_PD);
                }
                LUNATIC_CHECK(ReceiveMatrixData(driver, 5, 12) == data);
                driver.Quiesce();
                driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::Evict, 6, 13, line.address));
                driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
                break;
        }
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == state);
    }

    MatrixDriver::REQ PlainStashRequest(MatrixDriver& driver, bool unique, uint64_t address)
    {
        const uint16_t opcode = unique ? CHI::Eb::Opcodes::REQ::StashOnceUnique
            : CHI::Eb::Opcodes::REQ::StashOnceShared;
        auto request = driver.Request(opcode, 4, 20, address);
        request.ReturnNID() = 5;
        request.StashNIDValid() = 1;
        request.StashLPIDValid() = 1;
        request.StashLPID() = 3;
        return request;
    }

    MatrixDriver::REQ WriteStashRequest(MatrixDriver& driver, bool full, uint64_t address)
    {
        const uint16_t opcode = full ? CHI::Eb::Opcodes::REQ::WriteUniqueFullStash
            : CHI::Eb::Opcodes::REQ::WriteUniquePtlStash;
        auto request = driver.Request(opcode, 4, 30, address);
        request.ReturnNID() = 5;
        request.StashNIDValid() = 1;
        request.StashLPIDValid() = 1;
        request.StashLPID() = 3;
        return request;
    }

    void CompleteMatrixWrite(MatrixDriver& driver, bool full)
    {
        LineData update = MatrixPattern();
        for (uint8_t& byte : update)
            byte ^= 0xa5;
        const ByteEnables enables = MatrixEnables(!full);
        driver.driver.Until([&] { return static_cast<bool>(driver.driver.node.PeekTXRSP()); });
        const uint16_t firstOpcode = driver.driver.node.PeekTXRSP()->Opcode();
        if (firstOpcode == CHI::Eb::Opcodes::RSP::Comp)
        {
            LUNATIC_CHECK(driver.NextRSP(firstOpcode).TxnID() == 30);
            const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
            driver.WriteData(4, 30, dbid, update, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        }
        else if (firstOpcode == CHI::Eb::Opcodes::RSP::DBIDResp)
        {
            const DataBufferId dbid = driver.NextRSP(firstOpcode).DBID();
            driver.WriteData(4, 30, dbid, update, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 30);
        }
        else
        {
            LUNATIC_CHECK(firstOpcode == CHI::Eb::Opcodes::RSP::CompDBIDResp);
            const DataBufferId dbid = driver.NextRSP(firstOpcode).DBID();
            driver.WriteData(4, 30, dbid, update, enables, CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
        }
    }

    void CheckMatrixWrite(MatrixDriver& driver, const LineKey& line, bool full, CacheState initial)
    {
        LineData expected = MatrixPattern();
        const ByteEnables enables = MatrixEnables(!full);
        for (size_t index = 0; index < expected.size(); ++index)
            if (enables[index])
                expected[index] ^= 0xa5;
        const LineData actual = driver.driver.node.InspectHome(line).data;
        if (actual != expected)
        {
            for (size_t index = 0; index < expected.size(); ++index)
                if (actual[index] != expected[index])
                    throw std::runtime_error("write-stash data mismatch: state=" +
                        std::to_string(unsigned(initial)) + " index=" + std::to_string(index) +
                        " actual=" + std::to_string(actual[index]) +
                        " expected=" + std::to_string(expected[index]));
        }
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::I);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == CacheState::I);
    }

    void QuiesceMatrix(MatrixDriver& driver, bool expectError)
    {
        if (!expectError)
        {
            driver.Quiesce();
            return;
        }
        driver.driver.Quiesce();
        for (auto& entry : driver.oracles)
            LUNATIC_CHECK(!entry.second->HasInflight());
    }

    void RunPlainStash(CacheState initial, bool unique, uint8_t response, bool expectError = false)
    {
        Options options;
        options.outputQueueLimit = 1;
        MatrixDriver driver(options);
        const LineKey line = LineKey::FromAddress(0x56000, false);
        SeedMatrixTarget(driver, line, initial);
        driver.PushRequest(PlainStashRequest(driver, unique, line.address));
        const uint16_t opcode = unique ? CHI::Eb::Opcodes::SNP::SnpStashUnique
            : CHI::Eb::Opcodes::SNP::SnpStashShared;
        auto snoop = driver.NextSNP(opcode);
        LUNATIC_CHECK(snoop.target == 5 && snoop.flit.RetToSrc() == 0);
        driver.SnoopResponse(5, snoop.flit.TxnID(), response);
        LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).TxnID() == 20);
        QuiesceMatrix(driver, expectError);
        CacheState expected = initial;
        if (response == CHI::Eb::Resps::SnpResp_SC)
            expected = CacheState::SC;
        else if (response == CHI::Eb::Resps::SnpResp_SD)
            expected = CacheState::SD;
        else if (initial == CacheState::UC && response == CHI::Eb::Resps::SnpResp_UC)
            expected = CacheState::UD;
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == expected);
        LUNATIC_CHECK(driver.driver.node.GetErrors().empty() == !expectError);
    }

    void RawMatrixSnoopResponse(MatrixDriver& driver, TransactionId txnId, uint8_t resp)
    {
        MatrixDriver::RSP response{};
        response.SrcID() = 5;
        response.TgtID() = driver.home;
        response.TxnID() = txnId;
        response.Opcode() = CHI::Eb::Opcodes::RSP::SnpResp;
        response.Resp() = resp;
        LUNATIC_CHECK(driver.driver.node.PushRXRSP(response));
        driver.driver.Tick();
    }

    void RawMatrixSnoopData(MatrixDriver& driver, const LineKey& line, TransactionId txnId,
        const LineData& data, uint8_t resp, bool partial)
    {
        for (size_t beat = 0; beat < 2; ++beat)
        {
            MatrixDriver::DAT flit{};
            flit.SrcID() = 5;
            flit.TgtID() = driver.home;
            flit.TxnID() = txnId;
            flit.Opcode() = partial ? CHI::Eb::Opcodes::DAT::SnpRespDataPtl : CHI::Eb::Opcodes::DAT::SnpRespData;
            flit.Resp() = resp;
            flit.DataID() = beat * 2;
            flit.CCID() = (line.address >> 4) & 3;
            for (size_t index = 0; index < 32; ++index)
            {
                flit.Data()[index / 8] |= uint64_t(data[beat * 32 + index]) << ((index % 8) * 8);
                flit.BE() |= uint64_t(1) << index;
            }
            LUNATIC_CHECK(driver.driver.node.PushRXDAT(flit));
        }
        driver.driver.Tick();
    }

    void RunUniqueStash(CacheState initial, bool withData, bool partial, bool expectError = false)
    {
        MatrixDriver driver;
        const LineKey line = LineKey::FromAddress(0x56100, false);
        SeedMatrixTarget(driver, line, initial);
        driver.PushRequest(WriteStashRequest(driver, false, line.address));
        auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpUniqueStash);
        LUNATIC_CHECK(snoop.target == 5 && snoop.flit.RetToSrc() == 0);
        if (withData)
        {
            const uint8_t response = IsDirty(initial)
                ? partial ? CHI::Eb::Resps::SnpRespDataPtl_I_PD : CHI::Eb::Resps::SnpRespData_I_PD
                : CHI::Eb::Resps::SnpRespData_I;
            driver.SnoopData(5, snoop.flit.TxnID(), MatrixPattern(), response, partial);
        }
        else
            driver.SnoopResponse(5, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        if (expectError)
        {
            if (IsDirty(initial))
                RawMatrixSnoopData(driver, line, snoop.flit.TxnID(), MatrixPattern(), initial == CacheState::UDP
                    ? CHI::Eb::Resps::SnpRespDataPtl_I_PD : CHI::Eb::Resps::SnpRespData_I_PD, initial == CacheState::UDP);
            else
                RawMatrixSnoopResponse(driver, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        }
        CompleteMatrixWrite(driver, false);
        QuiesceMatrix(driver, expectError);
        CheckMatrixWrite(driver, line, false, initial);
        LUNATIC_CHECK(driver.driver.node.GetErrors().empty() == !expectError);
    }

    void RunMakeInvalidStash(CacheState initial, bool passDirty = false)
    {
        MatrixDriver driver;
        const LineKey line = LineKey::FromAddress(0x56200, false);
        SeedMatrixTarget(driver, line, initial);
        driver.PushRequest(WriteStashRequest(driver, true, line.address));
        auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash);
        LUNATIC_CHECK(snoop.target == 5 && snoop.flit.RetToSrc() == 0);
        const uint8_t response = passDirty
            ? CHI::Eb::Resps::SnpResp_I | CHI::Eb::Resps::PassDirty : CHI::Eb::Resps::SnpResp_I;
        driver.SnoopResponse(5, snoop.flit.TxnID(), response);
        if (passDirty)
            RawMatrixSnoopResponse(driver, snoop.flit.TxnID(), CHI::Eb::Resps::SnpResp_I);
        CompleteMatrixWrite(driver, true);
        QuiesceMatrix(driver, passDirty);
        CheckMatrixWrite(driver, line, true, initial);
        LUNATIC_CHECK(driver.driver.node.GetErrors().empty() == !passDirty);
    }

}

LUNATIC_TEST(Stage5PlainStashSnoopStateMatrix)
{
    const CacheState states[] = {
        CacheState::I, CacheState::UC, CacheState::UCE, CacheState::UD,
        CacheState::UDP, CacheState::SC, CacheState::SD
    };
    for (bool unique : { false, true })
        for (CacheState state : states)
        {
            RunPlainStash(state, unique, PreciseStashResponse(state));
            if (state != CacheState::I)
                RunPlainStash(state, unique, CHI::Eb::Resps::SnpResp_I);
        }
}

LUNATIC_TEST(Stage5UniqueStashSnoopResponseMatrix)
{
    struct ResponseCase {
        CacheState initial;
        bool       withData;
        bool       partial;
    };
    const ResponseCase cases[] = {
        { CacheState::I, false, false },
        { CacheState::UCE, false, false },
        { CacheState::UC, false, false },
        { CacheState::UC, true, false },
        { CacheState::SC, false, false },
        { CacheState::SC, true, false },
        { CacheState::UD, true, false },
        { CacheState::UDP, true, true },
        { CacheState::SD, true, false }
    };
    for (const ResponseCase& item : cases)
        RunUniqueStash(item.initial, item.withData, item.partial);
}

LUNATIC_TEST(Stage5MakeInvalidStashSnoopStateMatrix)
{
    const CacheState states[] = {
        CacheState::I, CacheState::UC, CacheState::UCE, CacheState::UD,
        CacheState::UDP, CacheState::SC, CacheState::SD
    };
    for (CacheState state : states)
        RunMakeInvalidStash(state);
}

LUNATIC_TEST(Stage5StashSnoopAcceptsSilentSharedState)
{
    RunPlainStash(CacheState::UC, true, CHI::Eb::Resps::SnpResp_SC);
}

LUNATIC_TEST(Stage5StashSnoopRejectsIllegalForms)
{
    RunUniqueStash(CacheState::I, true, false, true);
    RunUniqueStash(CacheState::UC, true, true, true);
    RunUniqueStash(CacheState::UD, true, true, true);
    RunMakeInvalidStash(CacheState::UD, true);
}
