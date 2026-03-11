#include "common.hpp"

#include <iostream>
#include <vector>

#include "tcpt_a_initial_read.hpp"
#include "tcpt_b_initial_dataless.hpp"
#include "tcpt_c_initial_write.hpp"
#include "tcpt_d_initial_atomic.hpp"
#include "tcpt_e_resp_read.hpp"
#include "tcpt_f_resp_dataless.hpp"
#include "tcpt_g_resp_write.hpp"
#include "tcpt_h_resp_atomic.hpp"
#include "tcpt_i_resp_snoop.hpp"
#include "tcpt_j_resp_snoopfwd.hpp"


void Hello() noexcept;
Xact::Topology GetTopologyPreset(bool print = false) noexcept;

int main(int argc, char* argv[])
{
    //
    size_t totalCount       = 0;
    size_t errCountFail     = 0;
    size_t errCountEnvError = 0;

    std::vector<std::string> errList;

    //
    Hello();

    //
    Xact::Topology topo = GetTopologyPreset(true);

    // TCPt A.
    TCPtAInitialRead(&totalCount, &errCountFail, &errCountEnvError, &errList, topo);

    // TCPt B.
    TCPtBInitialDataless(&totalCount, &errCountFail, &errCountEnvError, &errList, topo);

    // TCPt C.
    TCPtCInitialWrite(&totalCount, &errCountFail, &errCountEnvError, &errList, topo);

    // TCPt D.
    TCPtDInitialAtomic(&totalCount, &errCountFail, &errCountEnvError, &errList, topo);

    // TCPt E.
    TCPtERespRead(&totalCount, &errCountFail, &errCountEnvError, &errList, topo);

    // TCPt F.
    TCPtFRespDataless(&totalCount, &errCountFail, &errCountEnvError, &errList, topo);

    // TCPt G.
    TCPtGRespWrite(&totalCount, &errCountFail, &errCountEnvError, &errList, topo);

    // TCPt H.
    TCPtHRespAtomic(&totalCount, &errCountFail, &errCountEnvError, &errList, topo);

    // TCPt I.
    TCPtIRespSnoop(&totalCount, &errCountFail, &errCountEnvError, &errList, topo);

    // TCPt J.
    TCPtJRespSnoopFwd(&totalCount, &errCountFail, &errCountEnvError, &errList, topo);

    // Print errors if any
    if (!errList.empty())
    {
        std::cout << std::endl;
        std::cout << "==== Errors ============================================" << "\n";
        for (auto& e : errList)
            std::cout << e << "\n";
        std::cout << "========================================================" << "\n";
        std::cout << std::endl;
    }

    // End
    std::cout << "Finished " << totalCount << " test case point(s)." << std::endl;
    std::cout << errCountFail << " fail(s), " << errCountEnvError << " external/environment error(s) in total." << std::endl;
    std::cout << std::endl;

    std::cout << "Final result: ";
    if (errCountFail)
        std::cout << STATE_FAIL;
    else if (errCountEnvError)
        std::cout << STATE_ENVERROR;
    else
        std::cout << STATE_PASS;
    std::cout << std::endl;

    std::cout << std::endl;

    return errCountFail + errCountEnvError;
}


void Hello() noexcept
{
    std::cout << "" << std::endl;
    std::cout << "'tc_xact_state'" << std::endl;
    std::cout << "  - " << "Test case collection for *CacheStateMap in <chi_xact_state.hpp>" << std::endl;
#ifdef GIT_HASH
    std::cout << "Git commit hash: " << GIT_HASH << "" << std::endl;
#endif
}

Xact::Topology GetTopologyPreset(bool print) noexcept
{
    /*
    * Topology preset: 
    *
    *   +------+      +------+                +------+
    *   | RN-F |      | HN-F |                | HN-F |
    *   +------+      +------+                +------+
    *      4    \    /   2                   /   12
    *            +--+                    +--+
    *            |XP|--------------------|XP|
    *            +--+                    +--+
    *           /    \                  /
    *   +------+      +------+  +------+
    *   | HN-I |      | SN-F |  | RN-F |
    *   +------+      +------+  +------+
    *      0             6         10
    */
    Xact::Topology topo;
    topo.SetNode(0, Xact::NodeType::HN_I, "#0 HN-I");
    topo.SetNode(2, Xact::NodeType::HN_F, "#2 HN-F");
    topo.SetNode(4, Xact::NodeType::RN_F, "#4 RN-F");
    topo.SetNode(6, Xact::NodeType::SN_F, "#6 SN-F");
    topo.SetNode(10, Xact::NodeType::RN_F, "#10 RN-F");
    topo.SetNode(12, Xact::NodeType::HN_F, "#12 HN-F");

    if (print)
    {
        std::cout << "\n"
            " Topology preset: \n"
            "\n"
            "   +------+      +------+                +------+\n"
            "   | RN-F |      | HN-F |                | HN-F |\n"
            "   +------+      +------+                +------+\n"
            "      4    \\    /   2                  /   12\n"
            "            +--+                    +--+\n"
            "            |XP|--------------------|XP|\n"
            "            +--+                    +--+\n"
            "           /    \\                  /\n"
            "   +------+      +------+  +------+\n"
            "   | HN-I |      | SN-F |  | RN-F |\n"
            "   +------+      +------+  +------+\n"
            "      0             6         10\n"
            "\n" << std::endl;

        std::cout << "External SAM not present. Node ID must be precise.\n" << std::endl;
    }

    return topo;
}
