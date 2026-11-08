#include "common.hpp"

#include <algorithm>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    struct LegalRow {
        CacheState             initial;
        OrdinarySnoopResponse response;
    };

    void Add(std::vector<LegalRow>& table, CacheState initial, uint16_t opcode, uint8_t resp,
        bool withData = false, bool partial = false, int retToSrc = -1, int doNotGoToSD = -1)
    {
        const int lastRet = retToSrc < 0 ? 1 : retToSrc;
        const int lastDoNot = doNotGoToSD < 0 ? 1 : doNotGoToSD;
        for (int ret = retToSrc < 0 ? 0 : retToSrc; ret <= lastRet; ++ret)
            for (int doNot = doNotGoToSD < 0 ? 0 : doNotGoToSD; doNot <= lastDoNot; ++doNot)
                table.push_back({ initial, { opcode, resp, 0, withData, partial, bool(ret), bool(doNot) } });
    }

    void Alias(std::vector<LegalRow>& table, uint16_t source, uint16_t alias, bool zeroRetToSrc = false)
    {
        const size_t originalSize = table.size();
        for (size_t index = 0; index < originalSize; ++index)
        {
            if (table[index].response.opcode != source ||
                (zeroRetToSrc && table[index].response.retToSrc))
                continue;
            LegalRow row = table[index];
            row.response.opcode = alias;
            table.push_back(row);
        }
    }

    std::vector<LegalRow> BuildTable()
    {
        namespace R = CHI::Eb::Resps;
        namespace SNP = CHI::Eb::Opcodes::SNP;
        std::vector<LegalRow> table;

        Add(table, CacheState::I, SNP::SnpOnce, R::SnpResp_I);
        Add(table, CacheState::UC, SNP::SnpOnce, R::SnpResp_UC);
        Add(table, CacheState::UC, SNP::SnpOnce, R::SnpRespData_UC, true);
        Add(table, CacheState::UC, SNP::SnpOnce, R::SnpRespData_SC, true);
        Add(table, CacheState::UC, SNP::SnpOnce, R::SnpResp_I);
        Add(table, CacheState::UC, SNP::SnpOnce, R::SnpRespData_I, true);
        Add(table, CacheState::UCE, SNP::SnpOnce, R::SnpResp_UC);
        Add(table, CacheState::UCE, SNP::SnpOnce, R::SnpResp_I);
        for (uint8_t resp : { R::SnpRespData_UD, R::SnpRespData_SD, R::SnpRespData_SC_PD, R::SnpRespData_I_PD })
            Add(table, CacheState::UD, SNP::SnpOnce, resp, true);
        Add(table, CacheState::UDP, SNP::SnpOnce, R::SnpRespDataPtl_I_PD, true, true);
        Add(table, CacheState::UDP, SNP::SnpOnce, R::SnpRespDataPtl_UD, true, true);
        Add(table, CacheState::SC, SNP::SnpOnce, R::SnpResp_SC, false, false, 0);
        Add(table, CacheState::SC, SNP::SnpOnce, R::SnpResp_I, false, false, 0);
        Add(table, CacheState::SC, SNP::SnpOnce, R::SnpRespData_SC, true, false, 1);
        Add(table, CacheState::SC, SNP::SnpOnce, R::SnpRespData_I, true, false, 1);
        for (uint8_t resp : { R::SnpRespData_SD, R::SnpRespData_SC_PD, R::SnpRespData_I_PD })
            Add(table, CacheState::SD, SNP::SnpOnce, resp, true);
        Alias(table, SNP::SnpOnce, SNP::SnpOnceFwd, true);

        Add(table, CacheState::I, SNP::SnpShared, R::SnpResp_I);
        Add(table, CacheState::UC, SNP::SnpShared, R::SnpResp_SC);
        Add(table, CacheState::UC, SNP::SnpShared, R::SnpRespData_SC, true);
        Add(table, CacheState::UC, SNP::SnpShared, R::SnpResp_I);
        Add(table, CacheState::UC, SNP::SnpShared, R::SnpRespData_I, true);
        Add(table, CacheState::UCE, SNP::SnpShared, R::SnpResp_I);
        Add(table, CacheState::UD, SNP::SnpShared, R::SnpRespData_SD, true, false, -1, 0);
        Add(table, CacheState::UD, SNP::SnpShared, R::SnpRespData_SC_PD, true);
        Add(table, CacheState::UD, SNP::SnpShared, R::SnpRespData_I_PD, true);
        Add(table, CacheState::UDP, SNP::SnpShared, R::SnpRespDataPtl_I_PD, true, true);
        Add(table, CacheState::SC, SNP::SnpShared, R::SnpResp_SC, false, false, 0);
        Add(table, CacheState::SC, SNP::SnpShared, R::SnpResp_I, false, false, 0);
        Add(table, CacheState::SC, SNP::SnpShared, R::SnpRespData_SC, true, false, 1);
        Add(table, CacheState::SC, SNP::SnpShared, R::SnpRespData_I, true, false, 1);
        Add(table, CacheState::SD, SNP::SnpShared, R::SnpRespData_SD, true);
        Add(table, CacheState::SD, SNP::SnpShared, R::SnpRespData_SC_PD, true);
        Add(table, CacheState::SD, SNP::SnpShared, R::SnpRespData_I_PD, true);
        for (uint16_t alias : { SNP::SnpClean, SNP::SnpNotSharedDirty, SNP::SnpCleanFwd,
                 SNP::SnpSharedFwd, SNP::SnpNotSharedDirtyFwd })
            Alias(table, SNP::SnpShared, alias);

        Add(table, CacheState::I, SNP::SnpUnique, R::SnpResp_I);
        Add(table, CacheState::UC, SNP::SnpUnique, R::SnpResp_I);
        Add(table, CacheState::UC, SNP::SnpUnique, R::SnpRespData_I, true);
        Add(table, CacheState::UCE, SNP::SnpUnique, R::SnpResp_I);
        Add(table, CacheState::UD, SNP::SnpUnique, R::SnpRespData_I_PD, true);
        Add(table, CacheState::UDP, SNP::SnpUnique, R::SnpRespDataPtl_I_PD, true, true);
        Add(table, CacheState::SC, SNP::SnpUnique, R::SnpResp_I, false, false, 0);
        Add(table, CacheState::SC, SNP::SnpUnique, R::SnpRespData_I, true, false, 1);
        Add(table, CacheState::SD, SNP::SnpUnique, R::SnpRespData_I_PD, true);
        Alias(table, SNP::SnpUnique, SNP::SnpUniqueFwd, true);
        Alias(table, SNP::SnpUnique, SNP::SnpUniqueStash, true);
        Add(table, CacheState::SC, SNP::SnpUniqueStash, R::SnpRespData_I, true, false, 0);

        Alias(table, SNP::SnpShared, SNP::SnpPreferUnique);
        Alias(table, SNP::SnpUnique, SNP::SnpPreferUnique);
        Alias(table, SNP::SnpPreferUnique, SNP::SnpPreferUniqueFwd);

        Add(table, CacheState::I, SNP::SnpCleanShared, R::SnpResp_I, false, false, 0);
        Add(table, CacheState::UC, SNP::SnpCleanShared, R::SnpResp_UC, false, false, 0);
        Add(table, CacheState::UC, SNP::SnpCleanShared, R::SnpResp_SC, false, false, 0);
        Add(table, CacheState::UC, SNP::SnpCleanShared, R::SnpResp_I, false, false, 0);
        Add(table, CacheState::UCE, SNP::SnpCleanShared, R::SnpResp_I, false, false, 0);
        Add(table, CacheState::UD, SNP::SnpCleanShared, R::SnpRespData_UC_PD, true, false, 0);
        Add(table, CacheState::UD, SNP::SnpCleanShared, R::SnpRespData_SC_PD, true, false, 0);
        Add(table, CacheState::UD, SNP::SnpCleanShared, R::SnpRespData_I_PD, true, false, 0);
        Add(table, CacheState::UDP, SNP::SnpCleanShared, R::SnpRespDataPtl_I_PD, true, true, 0);
        Add(table, CacheState::SC, SNP::SnpCleanShared, R::SnpResp_SC, false, false, 0);
        Add(table, CacheState::SC, SNP::SnpCleanShared, R::SnpResp_I, false, false, 0);
        Add(table, CacheState::SD, SNP::SnpCleanShared, R::SnpRespData_SC_PD, true, false, 0);
        Add(table, CacheState::SD, SNP::SnpCleanShared, R::SnpRespData_I_PD, true, false, 0);

        for (CacheState state : { CacheState::I, CacheState::UC, CacheState::UCE, CacheState::SC })
            Add(table, state, SNP::SnpCleanInvalid, R::SnpResp_I, false, false, 0);
        Add(table, CacheState::UD, SNP::SnpCleanInvalid, R::SnpRespData_I_PD, true, false, 0);
        Add(table, CacheState::SD, SNP::SnpCleanInvalid, R::SnpRespData_I_PD, true, false, 0);
        Add(table, CacheState::UDP, SNP::SnpCleanInvalid, R::SnpRespDataPtl_I_PD, true, true, 0);

        for (CacheState state : { CacheState::I, CacheState::UC, CacheState::UCE, CacheState::UD,
                 CacheState::UDP, CacheState::SC, CacheState::SD })
        {
            Add(table, state, SNP::SnpMakeInvalid, R::SnpResp_I, false, false, 0);
            Add(table, state, SNP::SnpStashUnique, R::SnpResp_I, false, false, 0);
            Add(table, state, SNP::SnpStashShared, R::SnpResp_I, false, false, 0);
        }
        Alias(table, SNP::SnpMakeInvalid, SNP::SnpMakeInvalidStash, true);

        for (uint16_t opcode : { SNP::SnpStashUnique, SNP::SnpStashShared })
        {
            Add(table, CacheState::UC, opcode, R::SnpResp_UC, false, false, 0);
            Add(table, CacheState::UCE, opcode, R::SnpResp_UC, false, false, 0);
            Add(table, CacheState::UD, opcode, R::SnpResp_UD, false, false, 0);
            Add(table, CacheState::UDP, opcode, R::SnpResp_UD, false, false, 0);
            Add(table, CacheState::SC, opcode, R::SnpResp_SC, false, false, 0);
            Add(table, CacheState::SD, opcode, R::SnpResp_SD, false, false, 0);
        }

        Add(table, CacheState::I, SNP::SnpQuery, R::SnpResp_I, false, false, 0);
        Add(table, CacheState::UC, SNP::SnpQuery, R::SnpResp_UC, false, false, 0);
        Add(table, CacheState::UCE, SNP::SnpQuery, R::SnpResp_UC, false, false, 0);
        Add(table, CacheState::UD, SNP::SnpQuery, R::SnpResp_UD, false, false, 0);
        Add(table, CacheState::UDP, SNP::SnpQuery, R::SnpResp_UD, false, false, 0);
        Add(table, CacheState::SC, SNP::SnpQuery, R::SnpResp_SC, false, false, 0);
        Add(table, CacheState::SD, SNP::SnpQuery, R::SnpResp_SD, false, false, 0);
        return table;
    }

    bool Expected(const std::vector<LegalRow>& table, CacheState initial, const OrdinarySnoopResponse& response)
    {
        return std::any_of(table.begin(), table.end(), [&](const LegalRow& row) {
            return row.initial == initial && row.response.opcode == response.opcode &&
                row.response.resp == response.resp && row.response.withData == response.withData &&
                row.response.partial == response.partial && row.response.retToSrc == response.retToSrc &&
                row.response.doNotGoToSD == response.doNotGoToSD;
        });
    }

    void CheckEvaluation(bool expected, CacheState initial, const OrdinarySnoopResponse& response,
        const std::vector<CacheState>& possibleInitial)
    {
        const bool actual = IsOrdinarySnoopResponseLegal(possibleInitial, response);
        if (actual == expected)
            return;
        throw std::runtime_error("snoop response mismatch: state=" + std::to_string(unsigned(initial)) +
            " opcode=" + std::to_string(response.opcode) + " resp=" + std::to_string(response.resp) +
            " data=" + std::to_string(response.withData) + " partial=" + std::to_string(response.partial) +
            " ret=" + std::to_string(response.retToSrc) + " doNotSD=" + std::to_string(response.doNotGoToSD) +
            " expected=" + std::to_string(expected) + " actual=" + std::to_string(actual));
    }

    OrdinarySnoopResponse MakeResponse(uint16_t opcode, uint8_t resp, bool withData = false,
        bool partial = false, bool retToSrc = false, bool doNotGoToSD = false)
    {
        return { opcode, resp, 0, withData, partial, retToSrc, doNotGoToSD };
    }

    void CheckStates(CacheState tracked, const std::vector<CacheState>& expected)
    {
        const auto actual = PossibleSnoopStates(tracked);
        LUNATIC_CHECK(actual.size() == expected.size());
        for (CacheState state : expected)
            LUNATIC_CHECK(std::find(actual.begin(), actual.end(), state) != actual.end());
    }

}

LUNATIC_TEST(Stage9OrdinarySnoopResponseSpecificationMatrix)
{
    const std::vector<LegalRow> table = BuildTable();
    const CacheState states[] = {
        CacheState::I, CacheState::UC, CacheState::UCE, CacheState::UD,
        CacheState::UDP, CacheState::SC, CacheState::SD
    };
    std::vector<uint16_t> opcodes;
    for (const LegalRow& row : table)
        if (std::find(opcodes.begin(), opcodes.end(), row.response.opcode) == opcodes.end())
            opcodes.push_back(row.response.opcode);

    for (uint16_t opcode : opcodes)
        for (CacheState initial : states)
            for (bool retToSrc : { false, true })
                for (bool doNotGoToSD : { false, true })
                    for (bool withData : { false, true })
                        for (bool partial : { false, true })
                            for (uint8_t resp = 0; resp < 8; ++resp)
                            {
                                const OrdinarySnoopResponse response{
                                    opcode, resp, 0, withData, partial, retToSrc, doNotGoToSD
                                };
                                CheckEvaluation(Expected(table, initial, response), initial, response, { initial });
                            }

    for (uint16_t opcode : opcodes)
    {
        OrdinarySnoopResponse response = MakeResponse(opcode, CHI::Eb::Resps::SnpResp_I);
        response.dataPull = 1;
        const bool stashSnoop = opcode == CHI::Eb::Opcodes::SNP::SnpUniqueStash ||
            opcode == CHI::Eb::Opcodes::SNP::SnpMakeInvalidStash ||
            opcode == CHI::Eb::Opcodes::SNP::SnpStashUnique ||
            opcode == CHI::Eb::Opcodes::SNP::SnpStashShared;
        CheckEvaluation(stashSnoop, CacheState::I, response, { CacheState::I });
    }
}

LUNATIC_TEST(Stage9OrdinarySnoopSilentTransitionMatrix)
{
    namespace R = CHI::Eb::Resps;
    namespace SNP = CHI::Eb::Opcodes::SNP;
    CheckStates(CacheState::I, { CacheState::I });
    CheckStates(CacheState::UC, { CacheState::UC, CacheState::UD, CacheState::SC, CacheState::SD, CacheState::I });
    CheckStates(CacheState::UCE, { CacheState::UCE, CacheState::UDP, CacheState::UD, CacheState::SD, CacheState::I });
    CheckStates(CacheState::UD, { CacheState::UD, CacheState::SD, CacheState::I });
    CheckStates(CacheState::UDP, { CacheState::UDP, CacheState::UD, CacheState::SD, CacheState::I });
    CheckStates(CacheState::SC, { CacheState::SC, CacheState::I });
    CheckStates(CacheState::SD, { CacheState::SD });

    const auto dirty = MakeResponse(SNP::SnpNotSharedDirty, R::SnpRespData_I_PD, true);
    CheckEvaluation(false, CacheState::UCE, dirty, { CacheState::UCE });
    CheckEvaluation(true, CacheState::UCE, dirty, PossibleSnoopStates(CacheState::UCE));
    CheckEvaluation(true, CacheState::UC, dirty, PossibleSnoopStates(CacheState::UC));
    CheckEvaluation(false, CacheState::SC, dirty, PossibleSnoopStates(CacheState::SC));

    const auto fullDirty = MakeResponse(SNP::SnpCleanInvalid, R::SnpRespData_I_PD, true);
    const auto partialDirty = MakeResponse(SNP::SnpCleanInvalid, R::SnpRespDataPtl_I_PD, true, true);
    CheckEvaluation(false, CacheState::UDP, fullDirty, { CacheState::UDP });
    CheckEvaluation(true, CacheState::UDP, fullDirty, PossibleSnoopStates(CacheState::UDP));
    CheckEvaluation(true, CacheState::UDP, partialDirty, { CacheState::UDP });
    CheckEvaluation(true, CacheState::UCE, partialDirty, PossibleSnoopStates(CacheState::UCE));

    const auto sharedDirty = MakeResponse(SNP::SnpShared, R::SnpRespData_SD, true, false, false, true);
    CheckEvaluation(false, CacheState::UD, sharedDirty, { CacheState::UD });
    CheckEvaluation(true, CacheState::UD, sharedDirty, PossibleSnoopStates(CacheState::UD));
    CheckEvaluation(true, CacheState::SD, sharedDirty, { CacheState::SD });

    const auto stashUnique = MakeResponse(SNP::SnpStashUnique, R::SnpResp_UC);
    CheckEvaluation(true, CacheState::UCE, stashUnique, { CacheState::UCE });
    CheckEvaluation(true, CacheState::UCE, stashUnique, PossibleSnoopStates(CacheState::UCE));
    CheckEvaluation(true, CacheState::UDP, stashUnique, { CacheState::UDP });
}
