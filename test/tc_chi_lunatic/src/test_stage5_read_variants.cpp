#include "oracle_driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    LineData ReadPattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 7);
        return data;
    }

    template<typename config>
    LineData ReadFull(CheckedDriver<config>& driver, NodeId source, TransactionId txnId,
                      const LineKey& line, uint16_t opcode, uint8_t expectedResp)
    {
        auto request = driver.Request(opcode, source, txnId, line.address, true);
        if (opcode == CHI::Eb::Opcodes::REQ::ReadOnceCleanInvalid ||
            opcode == CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid)
        {
            request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
            request.SnpAttr() = 1;
        }
        driver.PushRequest(request);

        LineData result{};
        DataBufferId dbid = 0;
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.Resp() == expectedResp);
            LUNATIC_CHECK(data.DataID() == beat * bytes / 16);
            dbid = data.DBID();
            for (size_t index = 0; index < bytes; ++index)
                result[beat * bytes + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
        }
        driver.CompAck(source, dbid);
        driver.Quiesce();
        return result;
    }

    template<typename config>
    void RunReadVariants(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<config> driver(options);
        const LineKey line = LineKey::FromAddress(0x30000, false);
        const LineData expected = ReadPattern(23);
        driver.driver.node.GetMemory().WriteFull(line, expected);

        struct Variant {
            uint16_t   opcode;
            uint8_t    response;
            CacheState final;
        };
        const Variant variants[] = {
            { CHI::Eb::Opcodes::REQ::ReadClean, CHI::Eb::Resps::CompData_SC, CacheState::SC },
            { CHI::Eb::Opcodes::REQ::ReadNotSharedDirty, CHI::Eb::Resps::CompData_SC, CacheState::SC },
            { CHI::Eb::Opcodes::REQ::ReadPreferUnique, CHI::Eb::Resps::CompData_SC, CacheState::SC },
            { CHI::Eb::Opcodes::REQ::ReadOnceCleanInvalid, CHI::Eb::Resps::CompData_I, CacheState::I },
            { CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid, CHI::Eb::Resps::CompData_I, CacheState::I },
            { CHI::Eb::Opcodes::REQ::MakeReadUnique, CHI::Eb::Resps::CompData_UC, CacheState::UC }
        };

        TransactionId txnId = 1;
        for (const Variant& variant : variants)
        {
            LUNATIC_CHECK(ReadFull(driver, 4, txnId++, line, variant.opcode, variant.response) == expected);
            LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == variant.final);
            if (variant.opcode == CHI::Eb::Opcodes::REQ::MakeReadUnique)
            {
                LUNATIC_CHECK(ReadFull(driver, 4, txnId++, line, CHI::Eb::Opcodes::REQ::ReadShared,
                    CHI::Eb::Resps::CompData_SC) == expected);
                driver.PushRequest(driver.Request(variant.opcode, 4, txnId++, line.address, true));
                auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
                LUNATIC_CHECK(response.Resp() == CHI::Eb::Resps::Comp_UC);
                driver.CompAck(4, response.DBID());
                driver.Quiesce();
                LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::UC);
            }
        }
    }

    template<typename config>
    void RunDirtyVariants(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<config> driver(options);
        const LineKey line = LineKey::FromAddress(0x31000, false);
        const LineData initial = ReadPattern(31);
        const LineData dirty = ReadPattern(117);
        driver.driver.node.GetMemory().WriteFull(line, initial);

        ByteEnables fullEnables{};
        fullEnables.fill(1);
        auto restoreDirty = [&](TransactionId txnId) {
            driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 5, txnId, line.address));
            auto writeResponse = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
            driver.WriteData(5, txnId, writeResponse.DBID(), dirty, fullEnables,
                CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
            driver.Quiesce();
            LUNATIC_CHECK(driver.ReadData(5, txnId + 1, line.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dirty);
        };
        restoreDirty(1);
        struct Variant {
            uint16_t   request;
            uint16_t   snoop;
            uint8_t    snoopResp;
            uint8_t    response;
            CacheState final;
            CacheState other;
        };
        const Variant variants[] = {
            { CHI::Eb::Opcodes::REQ::ReadClean, CHI::Eb::Opcodes::SNP::SnpClean,
              CHI::Eb::Resps::SnpRespData_SD, CHI::Eb::Resps::CompData_SC, CacheState::SC, CacheState::SD },
            { CHI::Eb::Opcodes::REQ::ReadNotSharedDirty, CHI::Eb::Opcodes::SNP::SnpNotSharedDirty,
              CHI::Eb::Resps::SnpRespData_SD, CHI::Eb::Resps::CompData_SC, CacheState::SC, CacheState::SD },
            { CHI::Eb::Opcodes::REQ::ReadPreferUnique, CHI::Eb::Opcodes::SNP::SnpPreferUnique,
              CHI::Eb::Resps::SnpRespData_I_PD, CHI::Eb::Resps::CompData_SC, CacheState::SC, CacheState::I },
            { CHI::Eb::Opcodes::REQ::ReadOnceCleanInvalid, CHI::Eb::Opcodes::SNP::SnpUnique,
              CHI::Eb::Resps::SnpRespData_I_PD, CHI::Eb::Resps::CompData_I, CacheState::I, CacheState::I },
            { CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid, CHI::Eb::Opcodes::SNP::SnpUnique,
              CHI::Eb::Resps::SnpRespData_I_PD, CHI::Eb::Resps::CompData_I, CacheState::I, CacheState::I },
            { CHI::Eb::Opcodes::REQ::MakeReadUnique, CHI::Eb::Opcodes::SNP::SnpCleanInvalid,
              CHI::Eb::Resps::SnpRespData_I_PD, CHI::Eb::Resps::CompData_UD_PD, CacheState::UD, CacheState::I }
        };

        TransactionId txnId = 3;
        for (const Variant& variant : variants)
        {
            auto request = driver.Request(variant.request, 4, txnId++, line.address, true);
            if (variant.request == CHI::Eb::Opcodes::REQ::ReadOnceCleanInvalid ||
                variant.request == CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid)
            {
                request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
                request.SnpAttr() = 1;
            }
            driver.PushRequest(request);
            auto snoop = driver.NextSNP(variant.snoop);
            driver.SnoopData(5, snoop.flit.TxnID(), dirty, variant.snoopResp);
            LineData actual{};
            DataBufferId dbid = 0;
            constexpr size_t bytes = config::dataWidth / 8;
            for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
            {
                auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
                LUNATIC_CHECK(data.Resp() == variant.response);
                dbid = data.DBID();
                for (size_t index = 0; index < bytes; ++index)
                    actual[beat * bytes + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
            }
            driver.CompAck(4, dbid);
            if (!driver.driver.node.GetErrors().empty())
                throw std::runtime_error(driver.driver.node.GetErrors().front().reason);
            driver.Quiesce();
            LUNATIC_CHECK(actual == dirty);
            LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == variant.final);
            LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 5) == variant.other);
            driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::Evict, 4, txnId++, line.address));
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).Resp() == CHI::Eb::Resps::Comp_I);
            driver.Quiesce();
            restoreDirty(txnId);
            txnId += 2;
        }
    }

}

LUNATIC_TEST(Stage5ReadVariants128)
{
    RunReadVariants<Config128>(PolicyKind::NINE);
    RunReadVariants<Config128>(PolicyKind::Exclusive);
    RunReadVariants<Config128>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5ReadVariants256)
{
    RunReadVariants<Config256>(PolicyKind::NINE);
    RunReadVariants<Config256>(PolicyKind::Exclusive);
    RunReadVariants<Config256>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5ReadVariants512)
{
    RunReadVariants<Config512>(PolicyKind::NINE);
    RunReadVariants<Config512>(PolicyKind::Exclusive);
    RunReadVariants<Config512>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5DirtyReadVariantsAllPolicies)
{
    RunDirtyVariants<Config256>(PolicyKind::NINE);
    RunDirtyVariants<Config256>(PolicyKind::Exclusive);
    RunDirtyVariants<Config256>(PolicyKind::Inclusive);
}

LUNATIC_TEST(Stage5ReadCleanRetainsDirtyRequester)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.slcSets = 1;
    options.slcWays = 1;
    CheckedDriver<Config256> driver(options);
    const LineKey a = LineKey::FromAddress(0x31100, false);
    const LineKey b = LineKey::FromAddress(0x31140, false);
    const LineData initial = ReadPattern(31);
    const LineData dirty = ReadPattern(117);
    const LineData replacement = ReadPattern(203);
    ByteEnables fullEnables{};
    fullEnables.fill(1);
    driver.driver.node.GetMemory().WriteFull(a, initial);

    const auto writeback = [&](NodeId source, TransactionId txnId, const LineKey& line, const LineData& data) {
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, source, txnId, line.address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(source, txnId, response.DBID(), data, fullEnables,
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
    };

    LUNATIC_CHECK(driver.ReadData(4, 1, a.address, CHI::Eb::Opcodes::REQ::ReadUnique) == initial);
    driver.Quiesce();
    writeback(4, 2, a, dirty);
    LUNATIC_CHECK(driver.ReadData(4, 3, a.address, CHI::Eb::Opcodes::REQ::ReadUnique) == dirty);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.node.InspectRequester(a, 4) == CacheState::UD);

    writeback(6, 4, b, replacement);
    LUNATIC_CHECK(!driver.driver.node.InspectHome(a).valid);
    LUNATIC_CHECK(driver.driver.node.InspectRequester(a, 4) == CacheState::UD);
    LUNATIC_CHECK(driver.driver.node.GetMemory().Read(a) == initial);

    (void)ReadFull(driver, 4, 5, a, CHI::Eb::Opcodes::REQ::ReadClean, CHI::Eb::Resps::CompData_SC);
    const CacheState retained = driver.driver.node.InspectRequester(a, 4);

    driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 5, 6, a.address, true));
    bool snooped = false;
    driver.driver.Until([&] {
        return driver.driver.node.PeekTXSNP() || driver.driver.node.PeekTXDAT();
    });
    if (driver.driver.node.PeekTXSNP())
    {
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
        LUNATIC_CHECK(snoop.target == 4);
        driver.SnoopData(4, snoop.flit.TxnID(), dirty, CHI::Eb::Resps::SnpRespData_SD);
        snooped = true;
    }

    LineData actual{};
    DataBufferId dbid = 0;
    for (size_t beat = 0; beat < 2; ++beat)
    {
        const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(data.Resp() == CHI::Eb::Resps::CompData_SC);
        dbid = data.DBID();
        for (size_t index = 0; index < 32; ++index)
            actual[beat * 32 + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
    }
    driver.CompAck(5, dbid);
    driver.Quiesce();

    LUNATIC_CHECK(actual == dirty);
    LUNATIC_CHECK(snooped);
    LUNATIC_CHECK(retained == CacheState::UD);
}

namespace {

    template<typename config>
    void CheckReadCleanUniqueState(PolicyKind policy, bool dirty)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<config> driver(options);
        const LineKey line = LineKey::FromAddress(0x31200, false);
        const LineData initial = ReadPattern(39);
        const LineData update = ReadPattern(131);
        driver.driver.node.GetMemory().WriteFull(line, initial);
        LUNATIC_CHECK(ReadFull(driver, 4, 1, line, CHI::Eb::Opcodes::REQ::ReadUnique,
            CHI::Eb::Resps::CompData_UC) == initial);
        if (dirty)
        {
            ByteEnables enables{};
            enables.fill(1);
            driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 2, line.address));
            const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
            driver.WriteData(4, 2, response.DBID(), update, enables,
                CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
            driver.Quiesce();
            LUNATIC_CHECK(ReadFull(driver, 4, 3, line, CHI::Eb::Opcodes::REQ::ReadUnique,
                CHI::Eb::Resps::CompData_UD_PD) == update);
        }
        LUNATIC_CHECK(ReadFull(driver, 4, 4, line, CHI::Eb::Opcodes::REQ::ReadClean,
            CHI::Eb::Resps::CompData_SC) == (dirty ? update : initial));
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) ==
            (dirty ? CacheState::UD : CacheState::UC));
    }

    template<typename config>
    void CheckReadCleanPartialDirtyState(PolicyKind policy)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<config> driver(options);
        const LineKey line = LineKey::FromAddress(0x31300, false);
        const LineData initial = ReadPattern(47);
        const LineData update = ReadPattern(139);
        LineData expected = initial;
        ByteEnables enables{};
        for (size_t index = 5; index < 21; ++index)
        {
            enables[index] = 1;
            expected[index] = update[index];
        }
        driver.driver.node.GetMemory().WriteFull(line, initial);
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackPtl, 4, 1, line.address));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(4, 1, response.DBID(), update, enables,
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(ReadFull(driver, 4, 2, line, CHI::Eb::Opcodes::REQ::ReadUnique,
            CHI::Eb::Resps::CompData_UD_PD) == expected);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::UDP);
        LUNATIC_CHECK(ReadFull(driver, 4, 3, line, CHI::Eb::Opcodes::REQ::ReadClean,
            CHI::Eb::Resps::CompData_SC) == expected);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::UD);
    }

    template<typename config>
    void CheckSharedDirtyState(PolicyKind policy, uint16_t opcode)
    {
        Options options;
        options.policy = policy;
        CheckedDriver<config> driver(options);
        const LineKey line = LineKey::FromAddress(0x31400, false);
        const LineData initial = ReadPattern(53);
        const LineData dirty = ReadPattern(149);
        driver.driver.node.GetMemory().WriteFull(line, initial);
        LUNATIC_CHECK(ReadFull(driver, 4, 1, line, CHI::Eb::Opcodes::REQ::ReadUnique,
            CHI::Eb::Resps::CompData_UC) == initial);
        ByteEnables enables{};
        enables.fill(1);
        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, 4, 2, line.address));
        const auto writeback = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        driver.WriteData(4, 2, writeback.DBID(), dirty, enables,
            CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD);
        driver.Quiesce();
        LUNATIC_CHECK(ReadFull(driver, 4, 3, line, CHI::Eb::Opcodes::REQ::ReadUnique,
            CHI::Eb::Resps::CompData_UD_PD) == dirty);

        driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 5, 4, line.address, true));
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
        LUNATIC_CHECK(snoop.target == 4);
        driver.SnoopData(4, snoop.flit.TxnID(), dirty, CHI::Eb::Resps::SnpRespData_SD);
        LineData actual{};
        DataBufferId dbid = 0;
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
            LUNATIC_CHECK(data.Resp() == CHI::Eb::Resps::CompData_SC);
            dbid = data.DBID();
            for (size_t index = 0; index < bytes; ++index)
                actual[beat * bytes + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
        }
        LUNATIC_CHECK(actual == dirty);
        driver.CompAck(5, dbid);
        driver.Quiesce();
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::SD);

        if (policy == PolicyKind::Exclusive)
        {
            driver.PushRequest(driver.Request(opcode, 4, 5, line.address, true));
            const auto fallback = driver.NextSNP(opcode == CHI::Eb::Opcodes::REQ::ReadClean
                ? CHI::Eb::Opcodes::SNP::SnpClean : CHI::Eb::Opcodes::SNP::SnpPreferUnique);
            LUNATIC_CHECK(fallback.target == 5);
            driver.SnoopResponse(5, fallback.flit.TxnID(), CHI::Eb::Resps::SnpResp_SC);
            actual.fill(0);
            for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
            {
                const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
                LUNATIC_CHECK(data.Resp() == CHI::Eb::Resps::CompData_SC);
                dbid = data.DBID();
                for (size_t index = 0; index < bytes; ++index)
                    actual[beat * bytes + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
            }
            driver.CompAck(4, dbid);
            driver.Quiesce();
            LUNATIC_CHECK(actual == dirty);
        }
        else
            LUNATIC_CHECK(ReadFull(driver, 4, 5, line, opcode, CHI::Eb::Resps::CompData_SC) == dirty);
        LUNATIC_CHECK(driver.driver.node.InspectRequester(line, 4) == CacheState::SD);
    }

}

LUNATIC_TEST(Stage5ReadRetainedStateMatrix)
{
    for (PolicyKind policy : { PolicyKind::NINE, PolicyKind::Exclusive, PolicyKind::Inclusive })
    {
        CheckReadCleanUniqueState<Config256>(policy, false);
        CheckReadCleanUniqueState<Config256>(policy, true);
        CheckReadCleanPartialDirtyState<Config256>(policy);
        CheckSharedDirtyState<Config256>(policy, CHI::Eb::Opcodes::REQ::ReadClean);
        CheckSharedDirtyState<Config256>(policy, CHI::Eb::Opcodes::REQ::ReadPreferUnique);
    }
}
