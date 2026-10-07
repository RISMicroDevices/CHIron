//
// Boot script table definition. The table contents live in
// avocado_boot_script.inc (generated data — do not hand-edit).
//
#include "avocado_boot_script.h"


namespace Cohestra {

    const AvocadoBootEntry AVOCADO_BOOT_SCRIPT[] = {
    #include "avocado_boot_script.inc"
    };

    const std::size_t AVOCADO_BOOT_SCRIPT_COUNT =
        sizeof(AVOCADO_BOOT_SCRIPT) / sizeof(AVOCADO_BOOT_SCRIPT[0]);
}
