#include "common.hpp"

#include <algorithm>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    struct DCTRow {
        CacheState              initial;
        ForwardedSnoopResponse response;
    };

    void Add(std::vector<DCTRow>& table, CacheState initial, uint16_t opcode, uint8_t resp, uint8_t fwdState,
        bool withData, int retToSrc = -1, int doNotGoToSD = -1)
    {
        const int lastRet = retToSrc < 0 ? 1 : retToSrc;
        const int lastDoNot = doNotGoToSD < 0 ? 1 : doNotGoToSD;
        for (int ret = retToSrc < 0 ? 0 : retToSrc; ret <= lastRet; ++ret)
            for (int doNot = doNotGoToSD < 0 ? 0 : doNotGoToSD; doNot <= lastDoNot; ++doNot)
                table.push_back({ initial, { opcode, resp, fwdState, withData, bool(ret), bool(doNot) } });
    }

    void Alias(std::vector<DCTRow>& table, uint16_t source, uint16_t alias)
    {
        const size_t originalSize = table.size();
        for (size_t index = 0; index < originalSize; ++index)
            if (table[index].response.opcode == source)
            {
                DCTRow row = table[index];
                row.response.opcode = alias;
                table.push_back(row);
            }
    }

    void AddClean(std::vector<DCTRow>& table, uint16_t opcode)
    {
        namespace R = CHI::Eb::Resps;
        namespace F = CHI::Eb::FwdStates;
        for (CacheState state : { CacheState::UC, CacheState::SC })
            for (bool ret : { false, true })
            {
                Add(table, state, opcode, R::SnpResp_SC_Fwded_SC, F::SC, ret, ret);
                Add(table, state, opcode, R::SnpResp_I_Fwded_SC, F::SC, ret, ret);
            }
        Add(table, CacheState::UD, opcode, R::SnpResp_SD_Fwded_SC, F::SC, false, 0, 0);
        Add(table, CacheState::UD, opcode, R::SnpRespData_SD_Fwded_SC, F::SC, true, 1, 0);
        Add(table, CacheState::UD, opcode, R::SnpRespData_SC_PD_Fwded_SC, F::SC, true);
        Add(table, CacheState::UD, opcode, R::SnpRespData_I_PD_Fwded_SC, F::SC, true);
        Add(table, CacheState::SD, opcode, R::SnpResp_SD_Fwded_SC, F::SC, false, 0);
        Add(table, CacheState::SD, opcode, R::SnpRespData_SD_Fwded_SC, F::SC, true, 1);
        Add(table, CacheState::SD, opcode, R::SnpRespData_SC_PD_Fwded_SC, F::SC, true);
        Add(table, CacheState::SD, opcode, R::SnpRespData_I_PD_Fwded_SC, F::SC, true);
    }

    std::vector<DCTRow> BuildTable()
    {
        namespace R = CHI::Eb::Resps;
        namespace F = CHI::Eb::FwdStates;
        namespace SNP = CHI::Eb::Opcodes::SNP;
        std::vector<DCTRow> table;

        Add(table, CacheState::UC, SNP::SnpOnceFwd, R::SnpResp_UC_Fwded_I, F::I, false, 0);
        Add(table, CacheState::UC, SNP::SnpOnceFwd, R::SnpResp_SC_Fwded_I, F::I, false, 0);
        Add(table, CacheState::UC, SNP::SnpOnceFwd, R::SnpResp_I_Fwded_I, F::I, false, 0);
        Add(table, CacheState::UD, SNP::SnpOnceFwd, R::SnpResp_UD_Fwded_I, F::I, false, 0);
        Add(table, CacheState::UD, SNP::SnpOnceFwd, R::SnpResp_SD_Fwded_I, F::I, false, 0);
        Add(table, CacheState::UD, SNP::SnpOnceFwd, R::SnpRespData_SC_PD_Fwded_I, F::I, true, 0);
        Add(table, CacheState::UD, SNP::SnpOnceFwd, R::SnpRespData_I_PD_Fwded_I, F::I, true, 0);
        Add(table, CacheState::SC, SNP::SnpOnceFwd, R::SnpResp_SC_Fwded_I, F::I, false, 0);
        Add(table, CacheState::SC, SNP::SnpOnceFwd, R::SnpResp_I_Fwded_I, F::I, false, 0);
        Add(table, CacheState::SD, SNP::SnpOnceFwd, R::SnpResp_SD_Fwded_I, F::I, false, 0);
        Add(table, CacheState::SD, SNP::SnpOnceFwd, R::SnpRespData_SC_PD_Fwded_I, F::I, true, 0);
        Add(table, CacheState::SD, SNP::SnpOnceFwd, R::SnpRespData_I_PD_Fwded_I, F::I, true, 0);

        AddClean(table, SNP::SnpCleanFwd);
        Alias(table, SNP::SnpCleanFwd, SNP::SnpNotSharedDirtyFwd);

        for (CacheState state : { CacheState::UC, CacheState::SC })
            for (bool ret : { false, true })
            {
                Add(table, state, SNP::SnpSharedFwd, R::SnpResp_SC_Fwded_SC, F::SC, ret, ret);
                Add(table, state, SNP::SnpSharedFwd, R::SnpResp_I_Fwded_SC, F::SC, ret, ret);
            }
        Add(table, CacheState::UD, SNP::SnpSharedFwd, R::SnpResp_SD_Fwded_SC, F::SC, false, 0, 0);
        Add(table, CacheState::UD, SNP::SnpSharedFwd, R::SnpRespData_SD_Fwded_SC, F::SC, true, 1, 0);
        Add(table, CacheState::UD, SNP::SnpSharedFwd, R::SnpResp_SC_Fwded_SD_PD, F::SD_PD, false, 0);
        Add(table, CacheState::UD, SNP::SnpSharedFwd, R::SnpRespData_SC_Fwded_SD_PD, F::SD_PD, true, 1);
        Add(table, CacheState::UD, SNP::SnpSharedFwd, R::SnpResp_I_Fwded_SD_PD, F::SD_PD, false, 0);
        Add(table, CacheState::UD, SNP::SnpSharedFwd, R::SnpRespData_I_Fwded_SD_PD, F::SD_PD, true, 1);
        Add(table, CacheState::UD, SNP::SnpSharedFwd, R::SnpRespData_SC_PD_Fwded_SC, F::SC, true);
        Add(table, CacheState::UD, SNP::SnpSharedFwd, R::SnpRespData_I_PD_Fwded_SC, F::SC, true);
        Add(table, CacheState::SD, SNP::SnpSharedFwd, R::SnpResp_SD_Fwded_SC, F::SC, false, 0);
        Add(table, CacheState::SD, SNP::SnpSharedFwd, R::SnpRespData_SD_Fwded_SC, F::SC, true, 1);
        Add(table, CacheState::SD, SNP::SnpSharedFwd, R::SnpResp_SC_Fwded_SD_PD, F::SD_PD, false, 0);
        Add(table, CacheState::SD, SNP::SnpSharedFwd, R::SnpRespData_SC_Fwded_SD_PD, F::SD_PD, true, 1);
        Add(table, CacheState::SD, SNP::SnpSharedFwd, R::SnpResp_I_Fwded_SD_PD, F::SD_PD, false, 0);
        Add(table, CacheState::SD, SNP::SnpSharedFwd, R::SnpRespData_I_Fwded_SD_PD, F::SD_PD, true, 1);
        Add(table, CacheState::SD, SNP::SnpSharedFwd, R::SnpRespData_SC_PD_Fwded_SC, F::SC, true);
        Add(table, CacheState::SD, SNP::SnpSharedFwd, R::SnpRespData_I_PD_Fwded_SC, F::SC, true);

        for (CacheState state : { CacheState::UC, CacheState::SC })
        {
            Add(table, state, SNP::SnpUniqueFwd, R::SnpResp_I_Fwded_UC, F::UC, false, 0);
            Add(table, state, SNP::SnpPreferUniqueFwd, R::SnpResp_I_Fwded_UC, F::UC, false);
            Add(table, state, SNP::SnpPreferUniqueFwd, R::SnpResp_SC_Fwded_SC, F::SC, false, 0);
            Add(table, state, SNP::SnpPreferUniqueFwd, R::SnpRespData_SC_Fwded_SC, F::SC, true, 1);
        }
        for (CacheState state : { CacheState::UD, CacheState::SD })
        {
            Add(table, state, SNP::SnpUniqueFwd, R::SnpResp_I_Fwded_UD_PD, F::UD_PD, false, 0);
            Add(table, state, SNP::SnpPreferUniqueFwd, R::SnpResp_I_Fwded_UD_PD, F::UD_PD, false);
            Add(table, state, SNP::SnpPreferUniqueFwd, R::SnpRespData_SC_PD_Fwded_SC, F::SC, true);
        }
        Add(table, CacheState::UD, SNP::SnpPreferUniqueFwd, R::SnpResp_SD_Fwded_SC, F::SC, false, 0, 0);
        Add(table, CacheState::UD, SNP::SnpPreferUniqueFwd, R::SnpRespData_SD_Fwded_SC, F::SC, true, 1, 0);
        Add(table, CacheState::SD, SNP::SnpPreferUniqueFwd, R::SnpResp_SD_Fwded_SC, F::SC, false, 0);
        Add(table, CacheState::SD, SNP::SnpPreferUniqueFwd, R::SnpRespData_SD_Fwded_SC, F::SC, true, 1);
        return table;
    }

    bool Expected(const std::vector<DCTRow>& table, CacheState initial, const ForwardedSnoopResponse& response)
    {
        return std::any_of(table.begin(), table.end(), [&](const DCTRow& row) {
            return row.initial == initial && row.response.opcode == response.opcode &&
                row.response.resp == response.resp && row.response.fwdState == response.fwdState &&
                row.response.withData == response.withData && row.response.retToSrc == response.retToSrc &&
                row.response.doNotGoToSD == response.doNotGoToSD;
        });
    }

    std::vector<CacheState> SilentStates(CacheState tracked)
    {
        switch (tracked)
        {
            case CacheState::I: return { CacheState::I };
            case CacheState::UC: return { CacheState::UC, CacheState::UD, CacheState::SC, CacheState::SD, CacheState::I };
            case CacheState::UCE: return { CacheState::UCE, CacheState::UDP, CacheState::UD, CacheState::SD, CacheState::I };
            case CacheState::UD: return { CacheState::UD, CacheState::SD, CacheState::I };
            case CacheState::UDP: return { CacheState::UDP, CacheState::UD, CacheState::SD, CacheState::I };
            case CacheState::SC: return { CacheState::SC, CacheState::I };
            case CacheState::SD: return { CacheState::SD };
        }
        return {};
    }

    void Check(bool expected, CacheState tracked, const ForwardedSnoopResponse& response,
        const std::vector<CacheState>& possibleInitial)
    {
        const bool actual = IsForwardedSnoopResponseLegal(possibleInitial, response);
        if (actual == expected)
            return;
        throw std::runtime_error("DCT response mismatch: tracked=" + std::to_string(unsigned(tracked)) +
            " opcode=" + std::to_string(response.opcode) + " resp=" + std::to_string(response.resp) +
            " fwd=" + std::to_string(response.fwdState) + " data=" + std::to_string(response.withData) +
            " ret=" + std::to_string(response.retToSrc) + " doNotSD=" + std::to_string(response.doNotGoToSD) +
            " expected=" + std::to_string(expected) + " actual=" + std::to_string(actual));
    }

    std::vector<uint16_t> Opcodes(const std::vector<DCTRow>& table)
    {
        std::vector<uint16_t> result;
        for (const DCTRow& row : table)
            if (std::find(result.begin(), result.end(), row.response.opcode) == result.end())
                result.push_back(row.response.opcode);
        return result;
    }

}

LUNATIC_TEST(Stage9DCTResponseSpecificationMatrix)
{
    const auto table = BuildTable();
    const CacheState states[] = {
        CacheState::I, CacheState::UC, CacheState::UCE, CacheState::UD,
        CacheState::UDP, CacheState::SC, CacheState::SD
    };
    for (uint16_t opcode : Opcodes(table))
        for (CacheState initial : states)
            for (bool ret : { false, true })
                for (bool doNot : { false, true })
                    for (bool withData : { false, true })
                        for (uint8_t resp = 0; resp < 8; ++resp)
                            for (uint8_t fwd = 0; fwd < 8; ++fwd)
                            {
                                const ForwardedSnoopResponse response{ opcode, resp, fwd, withData, ret, doNot };
                                Check(Expected(table, initial, response), initial, response, { initial });
                            }
}

LUNATIC_TEST(Stage9DCTSilentTransitionMatrix)
{
    const auto table = BuildTable();
    const CacheState states[] = {
        CacheState::I, CacheState::UC, CacheState::UCE, CacheState::UD,
        CacheState::UDP, CacheState::SC, CacheState::SD
    };
    for (uint16_t opcode : Opcodes(table))
        for (CacheState tracked : states)
            for (bool ret : { false, true })
                for (bool doNot : { false, true })
                    for (bool withData : { false, true })
                        for (uint8_t resp = 0; resp < 8; ++resp)
                            for (uint8_t fwd = 0; fwd < 8; ++fwd)
                            {
                                const ForwardedSnoopResponse response{ opcode, resp, fwd, withData, ret, doNot };
                                bool expected = false;
                                for (CacheState initial : SilentStates(tracked))
                                    expected = expected || Expected(table, initial, response);
                                Check(expected, tracked, response, PossibleSnoopStates(tracked));
                            }
}
