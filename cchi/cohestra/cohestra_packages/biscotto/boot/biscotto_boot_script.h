#pragma once

#include <cstddef>
#include <cstdint>


namespace Cohestra {

    /*
    *   BiscottoBootEntry: one entry of a Biscotto boot script.
    *
    *   A boot script is the linearized mesh configuration write stream
    *   (32-bit accesses, low/high word pairs for 64-bit registers).
    *   Every entry carries verify=true: the sequencer performs an AXI
    *   read-back compare after each write.
    */
    struct BiscottoBootEntry {
        uint64_t    addr;       // config-space address (PERIPHBASE 0x6000_0000)
        uint32_t    value;      // 32-bit value to write
        bool        verify;     // perform an AXI read-back compare after the write
    };

    // Generated table (definition in biscotto_boot_script.cpp, contents in
    // biscotto_boot_script.inc). Do not hand-edit the .inc.
    extern const BiscottoBootEntry    BISCOTTO_BOOT_SCRIPT[];
    extern const std::size_t          BISCOTTO_BOOT_SCRIPT_COUNT;
}
