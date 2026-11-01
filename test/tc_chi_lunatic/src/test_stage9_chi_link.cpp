#include "common.hpp"

#include "cchi/cohestra/cohestra_v3/interface_chi_link.hpp"

using namespace Cohestra;

namespace {

    struct LinkMock {
        uint8_t chi_rn0_txlinkactivereq = 0;
        uint8_t chi_rn0_txlinkactiveack = 0;
        uint8_t chi_rn0_rxlinkactivereq = 0;
        uint8_t chi_rn0_rxlinkactiveack = 0;
        uint8_t chi_rn0_rxsactive = 0;
        uint8_t chi_rn0_syscoack = 0;
    };

}

LUNATIC_TEST(Stage9CHILinkControl)
{
    static_assert(V3CHILinkControl<LinkMock>::HasPort0());
    static_assert(!V3CHILinkControl<LinkMock>::HasPort1());

    LinkMock mock;
    V3CHILinkControl<LinkMock> control(&mock);
    control.Reset();
    LUNATIC_CHECK(!mock.chi_rn0_txlinkactiveack);
    LUNATIC_CHECK(!mock.chi_rn0_rxlinkactivereq);
    LUNATIC_CHECK(!mock.chi_rn0_rxsactive);
    LUNATIC_CHECK(!mock.chi_rn0_syscoack);
    LUNATIC_CHECK(!control.IsRun(0));

    control.SetActive(true);
    control.TickPreHandshake();
    LUNATIC_CHECK(mock.chi_rn0_rxlinkactivereq);
    LUNATIC_CHECK(mock.chi_rn0_rxsactive);
    LUNATIC_CHECK(mock.chi_rn0_syscoack);
    LUNATIC_CHECK(!mock.chi_rn0_txlinkactiveack);

    mock.chi_rn0_txlinkactivereq = 1;
    mock.chi_rn0_rxlinkactiveack = 1;
    control.TickPreHandshake();
    LUNATIC_CHECK(mock.chi_rn0_txlinkactiveack);
    LUNATIC_CHECK(control.IsRun(0));
    LUNATIC_CHECK(!control.IsRun(1));

    control.Reset();
    LUNATIC_CHECK(!mock.chi_rn0_txlinkactiveack);
    LUNATIC_CHECK(!mock.chi_rn0_rxlinkactivereq);
    LUNATIC_CHECK(!mock.chi_rn0_rxsactive);
    LUNATIC_CHECK(!mock.chi_rn0_syscoack);
    LUNATIC_CHECK(!control.IsRun(0));
}
