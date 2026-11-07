#include "chi_lunatic_snoop_response.hpp"

#include "../../../chi_eb/spec/chi_eb_protocol.hpp"

namespace CHI::Lunatic {
    namespace {

        bool Matches(const OrdinarySnoopResponse& response, uint8_t resp, bool withData, bool partial = false) noexcept
        {
            return response.resp == resp && response.withData == withData && response.partial == partial;
        }

        bool MatchesOnce(CacheState initial, const OrdinarySnoopResponse& response) noexcept
        {
            namespace R = CHI::Eb::Resps;
            switch (initial)
            {
                case CacheState::I:
                    return Matches(response, R::SnpResp_I, false);
                case CacheState::UC:
                    return Matches(response, R::SnpResp_UC, false) ||
                        Matches(response, R::SnpRespData_UC, true) ||
                        Matches(response, R::SnpRespData_SC, true) ||
                        Matches(response, R::SnpResp_I, false) ||
                        Matches(response, R::SnpRespData_I, true);
                case CacheState::UCE:
                    return Matches(response, R::SnpResp_UC, false) || Matches(response, R::SnpResp_I, false);
                case CacheState::UD:
                    return Matches(response, R::SnpRespData_UD, true) ||
                        Matches(response, R::SnpRespData_SD, true) ||
                        Matches(response, R::SnpRespData_SC_PD, true) ||
                        Matches(response, R::SnpRespData_I_PD, true);
                case CacheState::UDP:
                    return Matches(response, R::SnpRespDataPtl_I_PD, true, true) ||
                        Matches(response, R::SnpRespDataPtl_UD, true, true);
                case CacheState::SC:
                    return response.retToSrc
                        ? Matches(response, R::SnpRespData_SC, true) || Matches(response, R::SnpRespData_I, true)
                        : Matches(response, R::SnpResp_SC, false) || Matches(response, R::SnpResp_I, false);
                case CacheState::SD:
                    return Matches(response, R::SnpRespData_SD, true) ||
                        Matches(response, R::SnpRespData_SC_PD, true) ||
                        Matches(response, R::SnpRespData_I_PD, true);
            }
            return false;
        }

        bool MatchesShared(CacheState initial, const OrdinarySnoopResponse& response) noexcept
        {
            namespace R = CHI::Eb::Resps;
            switch (initial)
            {
                case CacheState::I:
                    return Matches(response, R::SnpResp_I, false);
                case CacheState::UC:
                    return Matches(response, R::SnpResp_SC, false) ||
                        Matches(response, R::SnpRespData_SC, true) ||
                        Matches(response, R::SnpResp_I, false) ||
                        Matches(response, R::SnpRespData_I, true);
                case CacheState::UCE:
                    return Matches(response, R::SnpResp_I, false);
                case CacheState::UD:
                    return (!response.doNotGoToSD && Matches(response, R::SnpRespData_SD, true)) ||
                        Matches(response, R::SnpRespData_SC_PD, true) ||
                        Matches(response, R::SnpRespData_I_PD, true);
                case CacheState::UDP:
                    return Matches(response, R::SnpRespDataPtl_I_PD, true, true);
                case CacheState::SC:
                    return response.retToSrc
                        ? Matches(response, R::SnpRespData_SC, true) || Matches(response, R::SnpRespData_I, true)
                        : Matches(response, R::SnpResp_SC, false) || Matches(response, R::SnpResp_I, false);
                case CacheState::SD:
                    return Matches(response, R::SnpRespData_SD, true) ||
                        Matches(response, R::SnpRespData_SC_PD, true) ||
                        Matches(response, R::SnpRespData_I_PD, true);
            }
            return false;
        }

        bool MatchesUnique(CacheState initial, const OrdinarySnoopResponse& response) noexcept
        {
            namespace R = CHI::Eb::Resps;
            switch (initial)
            {
                case CacheState::I:
                case CacheState::UCE:
                    return Matches(response, R::SnpResp_I, false);
                case CacheState::UC:
                    return Matches(response, R::SnpResp_I, false) || Matches(response, R::SnpRespData_I, true);
                case CacheState::UD:
                case CacheState::SD:
                    return Matches(response, R::SnpRespData_I_PD, true);
                case CacheState::UDP:
                    return Matches(response, R::SnpRespDataPtl_I_PD, true, true);
                case CacheState::SC:
                    return response.retToSrc
                        ? Matches(response, R::SnpRespData_I, true)
                        : Matches(response, R::SnpResp_I, false);
            }
            return false;
        }

        bool MatchesCleanShared(CacheState initial, const OrdinarySnoopResponse& response) noexcept
        {
            namespace R = CHI::Eb::Resps;
            switch (initial)
            {
                case CacheState::I:
                case CacheState::UCE:
                    return Matches(response, R::SnpResp_I, false);
                case CacheState::UC:
                    return Matches(response, R::SnpResp_UC, false) ||
                        Matches(response, R::SnpResp_SC, false) || Matches(response, R::SnpResp_I, false);
                case CacheState::UD:
                    return Matches(response, R::SnpRespData_UC_PD, true) ||
                        Matches(response, R::SnpRespData_SC_PD, true) ||
                        Matches(response, R::SnpRespData_I_PD, true);
                case CacheState::UDP:
                    return Matches(response, R::SnpRespDataPtl_I_PD, true, true);
                case CacheState::SC:
                    return Matches(response, R::SnpResp_SC, false) || Matches(response, R::SnpResp_I, false);
                case CacheState::SD:
                    return Matches(response, R::SnpRespData_SC_PD, true) ||
                        Matches(response, R::SnpRespData_I_PD, true);
            }
            return false;
        }

        bool MatchesCleanInvalid(CacheState initial, const OrdinarySnoopResponse& response) noexcept
        {
            namespace R = CHI::Eb::Resps;
            switch (initial)
            {
                case CacheState::I:
                case CacheState::UC:
                case CacheState::UCE:
                case CacheState::SC:
                    return Matches(response, R::SnpResp_I, false);
                case CacheState::UD:
                case CacheState::SD:
                    return Matches(response, R::SnpRespData_I_PD, true);
                case CacheState::UDP:
                    return Matches(response, R::SnpRespDataPtl_I_PD, true, true);
            }
            return false;
        }

        bool MatchesUniqueStash(CacheState initial, const OrdinarySnoopResponse& response) noexcept
        {
            namespace R = CHI::Eb::Resps;
            if (initial == CacheState::SC)
                return Matches(response, R::SnpResp_I, false) || Matches(response, R::SnpRespData_I, true);
            return MatchesUnique(initial, response);
        }

        bool MatchesStashRead(CacheState initial, const OrdinarySnoopResponse& response) noexcept
        {
            namespace R = CHI::Eb::Resps;
            if (Matches(response, R::SnpResp_I, false))
                return true;
            switch (initial)
            {
                case CacheState::I:
                    return false;
                case CacheState::UC:
                case CacheState::UCE:
                    return Matches(response, R::SnpResp_UC, false);
                case CacheState::UD:
                case CacheState::UDP:
                    return Matches(response, R::SnpResp_UD, false);
                case CacheState::SC:
                    return Matches(response, R::SnpResp_SC, false);
                case CacheState::SD:
                    return Matches(response, R::SnpResp_SD, false);
            }
            return false;
        }

        bool MatchesQuery(CacheState initial, const OrdinarySnoopResponse& response) noexcept
        {
            namespace R = CHI::Eb::Resps;
            switch (initial)
            {
                case CacheState::I: return Matches(response, R::SnpResp_I, false);
                case CacheState::UC:
                case CacheState::UCE: return Matches(response, R::SnpResp_UC, false);
                case CacheState::UD:
                case CacheState::UDP: return Matches(response, R::SnpResp_UD, false);
                case CacheState::SC: return Matches(response, R::SnpResp_SC, false);
                case CacheState::SD: return Matches(response, R::SnpResp_SD, false);
            }
            return false;
        }

        bool MatchesResponse(uint16_t opcode, CacheState initial, const OrdinarySnoopResponse& response) noexcept
        {
            namespace SNP = CHI::Eb::Opcodes::SNP;
            namespace R = CHI::Eb::Resps;
            switch (opcode)
            {
                case SNP::SnpOnce:
                case SNP::SnpOnceFwd:
                    return MatchesOnce(initial, response);
                case SNP::SnpClean:
                case SNP::SnpShared:
                case SNP::SnpNotSharedDirty:
                case SNP::SnpCleanFwd:
                case SNP::SnpSharedFwd:
                case SNP::SnpNotSharedDirtyFwd:
                    return MatchesShared(initial, response);
                case SNP::SnpPreferUnique:
                case SNP::SnpPreferUniqueFwd:
                    return MatchesShared(initial, response) || MatchesUnique(initial, response);
                case SNP::SnpUnique:
                case SNP::SnpUniqueFwd:
                    return MatchesUnique(initial, response);
                case SNP::SnpCleanShared:
                    return MatchesCleanShared(initial, response);
                case SNP::SnpCleanInvalid:
                    return MatchesCleanInvalid(initial, response);
                case SNP::SnpMakeInvalid:
                case SNP::SnpMakeInvalidStash:
                    return Matches(response, R::SnpResp_I, false);
                case SNP::SnpUniqueStash:
                    return MatchesUniqueStash(initial, response);
                case SNP::SnpStashUnique:
                case SNP::SnpStashShared:
                    return MatchesStashRead(initial, response);
                case SNP::SnpQuery:
                    return MatchesQuery(initial, response);
                default:
                    return false;
            }
        }

        bool FwdMatches(const ForwardedSnoopResponse& response, uint8_t resp, uint8_t fwdState,
            bool withData) noexcept
        {
            return response.resp == resp && response.fwdState == fwdState && response.withData == withData;
        }

        bool MatchesOnceFwd(CacheState initial, const ForwardedSnoopResponse& response) noexcept
        {
            namespace R = CHI::Eb::Resps;
            namespace F = CHI::Eb::FwdStates;
            switch (initial)
            {
                case CacheState::UC:
                    return FwdMatches(response, R::SnpResp_UC_Fwded_I, F::I, false) ||
                        FwdMatches(response, R::SnpResp_SC_Fwded_I, F::I, false) ||
                        FwdMatches(response, R::SnpResp_I_Fwded_I, F::I, false);
                case CacheState::UD:
                    return FwdMatches(response, R::SnpResp_UD_Fwded_I, F::I, false) ||
                        FwdMatches(response, R::SnpResp_SD_Fwded_I, F::I, false) ||
                        FwdMatches(response, R::SnpRespData_SC_PD_Fwded_I, F::I, true) ||
                        FwdMatches(response, R::SnpRespData_I_PD_Fwded_I, F::I, true);
                case CacheState::SC:
                    return FwdMatches(response, R::SnpResp_SC_Fwded_I, F::I, false) ||
                        FwdMatches(response, R::SnpResp_I_Fwded_I, F::I, false);
                case CacheState::SD:
                    return FwdMatches(response, R::SnpResp_SD_Fwded_I, F::I, false) ||
                        FwdMatches(response, R::SnpRespData_SC_PD_Fwded_I, F::I, true) ||
                        FwdMatches(response, R::SnpRespData_I_PD_Fwded_I, F::I, true);
                default:
                    return false;
            }
        }

        bool MatchesCleanFwd(CacheState initial, const ForwardedSnoopResponse& response) noexcept
        {
            namespace R = CHI::Eb::Resps;
            namespace F = CHI::Eb::FwdStates;
            switch (initial)
            {
                case CacheState::UC:
                case CacheState::SC:
                    return FwdMatches(response, R::SnpResp_SC_Fwded_SC, F::SC, response.retToSrc) ||
                        FwdMatches(response, R::SnpResp_I_Fwded_SC, F::SC, response.retToSrc);
                case CacheState::UD:
                    if (!response.doNotGoToSD &&
                        FwdMatches(response, R::SnpResp_SD_Fwded_SC, F::SC, response.retToSrc))
                        return true;
                    return FwdMatches(response, R::SnpRespData_SC_PD_Fwded_SC, F::SC, true) ||
                        FwdMatches(response, R::SnpRespData_I_PD_Fwded_SC, F::SC, true);
                case CacheState::SD:
                    if (FwdMatches(response, R::SnpResp_SD_Fwded_SC, F::SC, response.retToSrc))
                        return true;
                    return FwdMatches(response, R::SnpRespData_SC_PD_Fwded_SC, F::SC, true) ||
                        FwdMatches(response, R::SnpRespData_I_PD_Fwded_SC, F::SC, true);
                default:
                    return false;
            }
        }

        bool MatchesSharedFwd(CacheState initial, const ForwardedSnoopResponse& response) noexcept
        {
            namespace R = CHI::Eb::Resps;
            namespace F = CHI::Eb::FwdStates;
            switch (initial)
            {
                case CacheState::UC:
                case CacheState::SC:
                    return FwdMatches(response, R::SnpResp_SC_Fwded_SC, F::SC, response.retToSrc) ||
                        FwdMatches(response, R::SnpResp_I_Fwded_SC, F::SC, response.retToSrc);
                case CacheState::UD:
                    if (!response.doNotGoToSD &&
                        FwdMatches(response, R::SnpResp_SD_Fwded_SC, F::SC, response.retToSrc))
                        return true;
                    if (FwdMatches(response, R::SnpResp_SC_Fwded_SD_PD, F::SD_PD, response.retToSrc) ||
                        FwdMatches(response, R::SnpResp_I_Fwded_SD_PD, F::SD_PD, response.retToSrc))
                        return true;
                    return FwdMatches(response, R::SnpRespData_SC_PD_Fwded_SC, F::SC, true) ||
                        FwdMatches(response, R::SnpRespData_I_PD_Fwded_SC, F::SC, true);
                case CacheState::SD:
                    if (FwdMatches(response, R::SnpResp_SD_Fwded_SC, F::SC, response.retToSrc))
                        return true;
                    if (FwdMatches(response, R::SnpResp_SC_Fwded_SD_PD, F::SD_PD, response.retToSrc) ||
                        FwdMatches(response, R::SnpResp_I_Fwded_SD_PD, F::SD_PD, response.retToSrc))
                        return true;
                    return FwdMatches(response, R::SnpRespData_SC_PD_Fwded_SC, F::SC, true) ||
                        FwdMatches(response, R::SnpRespData_I_PD_Fwded_SC, F::SC, true);
                default:
                    return false;
            }
        }

        bool MatchesUniqueFwd(CacheState initial, const ForwardedSnoopResponse& response) noexcept
        {
            namespace R = CHI::Eb::Resps;
            namespace F = CHI::Eb::FwdStates;
            switch (initial)
            {
                case CacheState::UC:
                case CacheState::SC:
                    return FwdMatches(response, R::SnpResp_I_Fwded_UC, F::UC, false);
                case CacheState::UD:
                case CacheState::SD:
                    return FwdMatches(response, R::SnpResp_I_Fwded_UD_PD, F::UD_PD, false);
                default:
                    return false;
            }
        }

        bool MatchesPreferExclusiveFwd(CacheState initial, const ForwardedSnoopResponse& response) noexcept
        {
            namespace R = CHI::Eb::Resps;
            namespace F = CHI::Eb::FwdStates;
            switch (initial)
            {
                case CacheState::UC:
                case CacheState::SC:
                    return FwdMatches(response, R::SnpResp_SC_Fwded_SC, F::SC, response.retToSrc);
                case CacheState::UD:
                    if (!response.doNotGoToSD &&
                        FwdMatches(response, R::SnpResp_SD_Fwded_SC, F::SC, response.retToSrc))
                        return true;
                    return FwdMatches(response, R::SnpRespData_SC_PD_Fwded_SC, F::SC, true);
                case CacheState::SD:
                    if (FwdMatches(response, R::SnpResp_SD_Fwded_SC, F::SC, response.retToSrc))
                        return true;
                    return FwdMatches(response, R::SnpRespData_SC_PD_Fwded_SC, F::SC, true);
                default:
                    return false;
            }
        }

        bool MatchesForwardedResponse(uint16_t opcode, CacheState initial,
            const ForwardedSnoopResponse& response) noexcept
        {
            namespace SNP = CHI::Eb::Opcodes::SNP;
            switch (opcode)
            {
                case SNP::SnpOnceFwd:
                    return MatchesOnceFwd(initial, response);
                case SNP::SnpCleanFwd:
                case SNP::SnpNotSharedDirtyFwd:
                    return MatchesCleanFwd(initial, response);
                case SNP::SnpSharedFwd:
                    return MatchesSharedFwd(initial, response);
                case SNP::SnpUniqueFwd:
                    return MatchesUniqueFwd(initial, response);
                case SNP::SnpPreferUniqueFwd:
                    return MatchesPreferExclusiveFwd(initial, response) || MatchesUniqueFwd(initial, response);
                default:
                    return false;
            }
        }

        bool RetToSrcMustBeZero(uint16_t opcode) noexcept
        {
            namespace SNP = CHI::Eb::Opcodes::SNP;
            switch (opcode)
            {
                case SNP::SnpOnceFwd:
                case SNP::SnpUniqueFwd:
                case SNP::SnpCleanShared:
                case SNP::SnpCleanInvalid:
                case SNP::SnpMakeInvalid:
                case SNP::SnpQuery:
                case SNP::SnpUniqueStash:
                case SNP::SnpMakeInvalidStash:
                case SNP::SnpStashUnique:
                case SNP::SnpStashShared:
                    return true;
                default:
                    return false;
            }
        }

    }

    std::vector<CacheState> PossibleSnoopStates(CacheState tracked)
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

    bool IsOrdinarySnoopResponseLegal(const std::vector<CacheState>& possibleInitial,
        const OrdinarySnoopResponse& response) noexcept
    {
        namespace SNP = CHI::Eb::Opcodes::SNP;
        const bool hasDataSource = response.withData && response.opcode != SNP::SnpUniqueStash &&
            response.opcode != SNP::SnpMakeInvalidStash && response.opcode != SNP::SnpStashUnique &&
            response.opcode != SNP::SnpStashShared;
        if ((response.dataPull != 0 && !hasDataSource) ||
            (response.retToSrc && RetToSrcMustBeZero(response.opcode)))
            return false;
        for (CacheState initial : possibleInitial)
            if (MatchesResponse(response.opcode, initial, response))
                return true;
        return false;
    }

    bool IsForwardedSnoopResponseLegal(const std::vector<CacheState>& possibleInitial,
        const ForwardedSnoopResponse& response) noexcept
    {
        namespace SNP = CHI::Eb::Opcodes::SNP;
        if ((response.opcode == SNP::SnpOnceFwd || response.opcode == SNP::SnpUniqueFwd) && response.retToSrc)
            return false;
        for (CacheState initial : possibleInitial)
            if (MatchesForwardedResponse(response.opcode, initial, response))
                return true;
        return false;
    }

}
