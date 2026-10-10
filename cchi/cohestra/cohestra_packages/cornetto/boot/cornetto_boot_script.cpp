//
// Boot script table definition. The table contents live in
// cornetto_boot_script.inc (generated data — do not hand-edit).
//
#include "cornetto_boot_script.h"


namespace Cohestra {

    const CornettoBootEntry CORNETTO_BOOT_SCRIPT[] = {
    #include "cornetto_boot_script.inc"
    };

    const std::size_t CORNETTO_BOOT_SCRIPT_COUNT =
        sizeof(CORNETTO_BOOT_SCRIPT) / sizeof(CORNETTO_BOOT_SCRIPT[0]);
}
