#include "tc_common_resp.hpp"           // IWYU pragma: keep
#include "tcpt_i_resp_snoop.hpp"        // IWYU pragma: keep

#include "tcpt_i_resp_snoop_e/tcpt_01_04_SnpOnce.hpp"
#include "tcpt_i_resp_snoop_e/tcpt_05_08_SnpClean.hpp"
#include "tcpt_i_resp_snoop_e/tcpt_09_12_SnpShared.hpp"
#include "tcpt_i_resp_snoop_e/tcpt_13_16_SnpNotSharedDirty.hpp"
#include "tcpt_i_resp_snoop_e/tcpt_17_18_SnpUnique.hpp"
#include "tcpt_i_resp_snoop_e/tcpt_19_22_SnpPreferUnique.hpp"
#include "tcpt_i_resp_snoop_e/tcpt_23_24_SnpCleanShared.hpp"
#include "tcpt_i_resp_snoop_e/tcpt_25_26_SnpCleanInvalid.hpp"
#include "tcpt_i_resp_snoop_e/tcpt_27_28_SnpMakeInvalid.hpp"

#ifdef CHI_ISSUE_E

void TCPtIRespSnoop(
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

    TCPtIRespSnoop_SnpOnce(tcptResp);
    TCPtIRespSnoop_SnpClean(tcptResp);
    TCPtIRespSnoop_SnpShared(tcptResp);
    TCPtIRespSnoop_SnpNotSharedDirty(tcptResp);
    TCPtIRespSnoop_SnpUnique(tcptResp);
    TCPtIRespSnoop_SnpPreferUnique(tcptResp);
    TCPtIRespSnoop_SnpCleanShared(tcptResp);
    TCPtIRespSnoop_SnpCleanInvalid(tcptResp);
    TCPtIRespSnoop_SnpMakeInvalid(tcptResp);

    tcptResp->End();
    tcptResp = nullptr;
}

#endif
