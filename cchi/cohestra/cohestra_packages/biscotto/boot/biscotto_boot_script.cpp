//
// Boot script table definition. The table contents live in
// biscotto_boot_script.inc (generated data — do not hand-edit).
//
#include "biscotto_boot_script.h"


namespace Cohestra {

    const BiscottoBootEntry BISCOTTO_BOOT_SCRIPT[] = {
    #include "biscotto_boot_script.inc"
    };

    const std::size_t BISCOTTO_BOOT_SCRIPT_COUNT =
        sizeof(BISCOTTO_BOOT_SCRIPT) / sizeof(BISCOTTO_BOOT_SCRIPT[0]);
}
