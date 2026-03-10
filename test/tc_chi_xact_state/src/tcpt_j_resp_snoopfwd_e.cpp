#include "tc_common_resp.hpp"           // IWYU pragma: keep
#include "tcpt_i_resp_snoop.hpp"        // IWYU pragma: keep

#include "tcpt_j_resp_snoopfwd_e/tcpt_01_02_SnpOnceFwd.hpp"
#include "tcpt_j_resp_snoopfwd_e/tcpt_03_06_SnpCleanFwd.hpp"
#include "tcpt_j_resp_snoopfwd_e/tcpt_07_10_SnpNotSharedDirtyFwd.hpp"
#include "tcpt_j_resp_snoopfwd_e/tcpt_11_14_SnpSharedFwd.hpp"
#include "tcpt_j_resp_snoopfwd_e/tcpt_15_15_SnpUniqueFwd.hpp"
#include "tcpt_j_resp_snoopfwd_e/tcpt_16_19_SnpPreferUniqueFwd.hpp"

#ifdef CHI_ISSUE_E

void TCPtJRespSnoopFwd(
    size_t*                     totalCount,
    size_t*                     errCountFail,
    size_t*                     errCountEnvError,
    std::vector<std::string>*   errList,
    const Xact::Topology&       topo) noexcept
{
    TCPtResp* tcptResp = (new TCPtResp())
        ->TotalCount(totalCount)
        ->ErrCountFail(errCountFail)
        ->ErrCountEnvError(errCountEnvError)
        ->ErrList(errList)
        ->Topology(topo)
        ->Begin();

    TCPtJRespSnoopFwd_SnpOnceFwd(tcptResp);
    TCPtJRespSnoopFwd_SnpCleanFwd(tcptResp);
    TCPtJRespSnoopFwd_SnpNotSharedDirtyFwd(tcptResp);
    TCPtJRespSnoopFwd_SnpSharedFwd(tcptResp);
    TCPtJRespSnoopFwd_SnpUniqueFwd(tcptResp);
    TCPtJRespSnoopFwd_SnpPreferUniqueFwd(tcptResp);

    tcptResp->End();
    tcptResp = nullptr;
}

#endif
