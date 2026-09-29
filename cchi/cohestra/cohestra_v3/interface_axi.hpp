#pragma once

#include <concepts>                             // IWYU pragma: keep
#include <set>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "../cohestra_axi.hpp"
#include "interface.hpp"


namespace Cohestra {

    // TickPreHandshake (drive) -> Eval (clock=0) -> TickPostHandshake (retire/observe) -> Eval (clock=1)

    namespace details {

        // Narrow scalar pin (valid/ready, len/size/burst, id, addr, ...).
        template<class T>
        concept V3AXIMemoryScalarSignal = std::is_integral_v<std::remove_cvref_t<T>>;

        // Data/strobe pin: either a scalar (up to 64-bit) or a word array
        // (Verilator VlWide<N>, WData pointer wrappers or an unpacked
        // uint32_t array), with words addressed by operator[].
        template<class T>
        concept V3AXIMemoryDataSignal = std::is_integral_v<std::remove_cvref_t<T>>
            || requires (T t) { { t[0] } -> std::convertible_to<uint32_t&>; };

#define COHESTRA_V3_AXI_MEMORY_PORT_REQUIREMENTS(PORT) \
        { t.axi_m ## PORT ## _awvalid } -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _awready } -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _awid }    -> V3AXIMemoryScalarSignal; \
        { t.axi_m ## PORT ## _awaddr }  -> V3AXIMemoryScalarSignal; \
        { t.axi_m ## PORT ## _awlen }   -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _awsize }  -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _awburst } -> std::convertible_to<uint8_t&>; \
        \
        { t.axi_m ## PORT ## _wvalid }  -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _wready }  -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _wdata }   -> V3AXIMemoryDataSignal; \
        { t.axi_m ## PORT ## _wstrb }   -> V3AXIMemoryDataSignal; \
        { t.axi_m ## PORT ## _wlast }   -> std::convertible_to<uint8_t&>; \
        \
        { t.axi_m ## PORT ## _bvalid }  -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _bready }  -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _bid }     -> V3AXIMemoryScalarSignal; \
        { t.axi_m ## PORT ## _bresp }   -> std::convertible_to<uint8_t&>; \
        \
        { t.axi_m ## PORT ## _arvalid } -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _arready } -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _arid }    -> V3AXIMemoryScalarSignal; \
        { t.axi_m ## PORT ## _araddr }  -> V3AXIMemoryScalarSignal; \
        { t.axi_m ## PORT ## _arlen }   -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _arsize }  -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _arburst } -> std::convertible_to<uint8_t&>; \
        \
        { t.axi_m ## PORT ## _rvalid }  -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _rready }  -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _rid }     -> V3AXIMemoryScalarSignal; \
        { t.axi_m ## PORT ## _rdata }   -> V3AXIMemoryDataSignal; \
        { t.axi_m ## PORT ## _rresp }   -> std::convertible_to<uint8_t&>; \
        { t.axi_m ## PORT ## _rlast }   -> std::convertible_to<uint8_t&>;

#define COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_CONCEPTS(PORT) \
        template<class T> \
        concept V3AXIMemoryModulePort ## PORT = requires (T t) { \
            COHESTRA_V3_AXI_MEMORY_PORT_REQUIREMENTS(PORT) \
        };

        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_CONCEPTS(0)
        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_CONCEPTS(1)
        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_CONCEPTS(2)
        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_CONCEPTS(3)
        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_CONCEPTS(4)
        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_CONCEPTS(5)
        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_CONCEPTS(6)
        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_CONCEPTS(7)

#undef COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_CONCEPTS
#undef COHESTRA_V3_AXI_MEMORY_PORT_REQUIREMENTS

        // Word accessors over a data signal.
        // The word index addressing is bounds-safe for scalars; for word
        // arrays the callers must stay inside the signal's word count, which
        // is implied by the compile-time data width of the port traits.
        template<class T>
        inline uint32_t V3AXIMemoryDataWord(const T& data, size_t word) noexcept
        {
            if constexpr (std::is_integral_v<std::remove_cvref_t<T>>)
            {
                constexpr size_t bits = sizeof(std::remove_cvref_t<T>) * 8;

                if (word * 32 >= bits)
                    return 0;

                return uint32_t(data >> (word * 32));
            }
            else
                return data[word];
        }

        template<class T>
        inline void V3AXIMemoryDataSetWord(T& data, size_t word, uint32_t value) noexcept
        {
            if constexpr (std::is_integral_v<std::remove_cvref_t<T>>)
            {
                constexpr size_t bits = sizeof(std::remove_cvref_t<T>) * 8;

                if (word * 32 >= bits)
                    return;

                if constexpr (bits > 32)
                {
                    size_t shift = word * 32;
                    data = (data & ~(uint64_t(0xFFFFFFFF) << shift))
                         | (uint64_t(value) << shift);
                }
                else
                    data = value;
            }
            else
                data[word] = value;
        }

        // 64-bit lane accessors over a data signal.
        template<class T>
        inline uint64_t V3AXIMemoryDataLane(const T& data, size_t lane) noexcept
        {
            return uint64_t(V3AXIMemoryDataWord(data, lane * 2))
                | (uint64_t(V3AXIMemoryDataWord(data, lane * 2 + 1)) << 32);
        }

        template<class T>
        inline void V3AXIMemoryDataSetLane(T& data, size_t lane, uint64_t value) noexcept
        {
            V3AXIMemoryDataSetWord(data, lane * 2,     uint32_t(value));
            V3AXIMemoryDataSetWord(data, lane * 2 + 1, uint32_t(value >> 32));
        }

        // Compile-time traits of an AXI memory port, defined only for ports
        // whose pins are present on the module.
        // 'DataBits' derives the exact data signal width from the pin storage
        // type, which is exact for the supported widths (a 64-bit port is a
        // uint64_t scalar, wider ports are word arrays of exactly
        // DataBits / 32 words), and the static_asserts restrict the port to
        // the supported AXI memory data widths at compile time.
        template<class VT, size_t PORT>
        struct V3AXIMemoryPortTraits;

#define COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_TRAITS(PORT) \
        template<class VT> \
            requires V3AXIMemoryModulePort ## PORT<VT> \
        struct V3AXIMemoryPortTraits<VT, PORT> { \
            static constexpr size_t DataBits \
                = sizeof(std::remove_cvref_t<decltype(std::declval<VT&>().axi_m ## PORT ## _wdata)>) * 8; \
            static constexpr size_t StrbBits \
                = sizeof(std::remove_cvref_t<decltype(std::declval<VT&>().axi_m ## PORT ## _wstrb)>) * 8; \
            static constexpr size_t RDataBits \
                = sizeof(std::remove_cvref_t<decltype(std::declval<VT&>().axi_m ## PORT ## _rdata)>) * 8; \
            \
            static_assert(DataBits == 64 || DataBits == 128 || DataBits == 256 \
                       || DataBits == 512 || DataBits == 1024, \
                "V3AXISlaveInterface: AXI memory port data width must be one of 64, 128, 256, 512, 1024"); \
            static_assert(StrbBits * 8 == DataBits, \
                "V3AXISlaveInterface: AXI memory port strobe width must be the data width divided by 8"); \
            static_assert(RDataBits == DataBits, \
                "V3AXISlaveInterface: AXI memory port read data width must match the write data width"); \
        };

        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_TRAITS(0)
        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_TRAITS(1)
        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_TRAITS(2)
        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_TRAITS(3)
        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_TRAITS(4)
        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_TRAITS(5)
        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_TRAITS(6)
        COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_TRAITS(7)

#undef COHESTRA_V3_DECLARE_AXI_MEMORY_PORT_TRAITS

#define COHESTRA_V3_AXI_MASTER_PORT_REQUIREMENTS(PORT) \
        { t.axi_s ## PORT ## _awvalid } -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _awready } -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _awid }    -> V3AXIMemoryScalarSignal; \
        { t.axi_s ## PORT ## _awaddr }  -> V3AXIMemoryScalarSignal; \
        { t.axi_s ## PORT ## _awlen }   -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _awsize }  -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _awburst } -> std::convertible_to<uint8_t&>; \
        \
        { t.axi_s ## PORT ## _wvalid }  -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _wready }  -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _wdata }   -> V3AXIMemoryDataSignal; \
        { t.axi_s ## PORT ## _wstrb }   -> V3AXIMemoryDataSignal; \
        { t.axi_s ## PORT ## _wlast }   -> std::convertible_to<uint8_t&>; \
        \
        { t.axi_s ## PORT ## _bvalid }  -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _bready }  -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _bid }     -> V3AXIMemoryScalarSignal; \
        { t.axi_s ## PORT ## _bresp }   -> std::convertible_to<uint8_t&>; \
        \
        { t.axi_s ## PORT ## _arvalid } -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _arready } -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _arid }    -> V3AXIMemoryScalarSignal; \
        { t.axi_s ## PORT ## _araddr }  -> V3AXIMemoryScalarSignal; \
        { t.axi_s ## PORT ## _arlen }   -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _arsize }  -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _arburst } -> std::convertible_to<uint8_t&>; \
        \
        { t.axi_s ## PORT ## _rvalid }  -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _rready }  -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _rid }     -> V3AXIMemoryScalarSignal; \
        { t.axi_s ## PORT ## _rdata }   -> V3AXIMemoryDataSignal; \
        { t.axi_s ## PORT ## _rresp }   -> std::convertible_to<uint8_t&>; \
        { t.axi_s ## PORT ## _rlast }   -> std::convertible_to<uint8_t&>;

#define COHESTRA_V3_DECLARE_AXI_MASTER_PORT_CONCEPTS(PORT) \
        template<class T> \
        concept V3AXIMasterModulePort ## PORT = requires (T t) { \
            COHESTRA_V3_AXI_MASTER_PORT_REQUIREMENTS(PORT) \
        };

        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_CONCEPTS(0)
        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_CONCEPTS(1)
        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_CONCEPTS(2)
        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_CONCEPTS(3)
        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_CONCEPTS(4)
        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_CONCEPTS(5)
        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_CONCEPTS(6)
        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_CONCEPTS(7)

#undef COHESTRA_V3_DECLARE_AXI_MASTER_PORT_CONCEPTS
#undef COHESTRA_V3_AXI_MASTER_PORT_REQUIREMENTS

        // Compile-time traits of an AXI master port, defined only for ports
        // whose pins are present on the module. Same width derivation and
        // restrictions as details::V3AXIMemoryPortTraits.
        template<class VT, size_t PORT>
        struct V3AXIMasterPortTraits;

#define COHESTRA_V3_DECLARE_AXI_MASTER_PORT_TRAITS(PORT) \
        template<class VT> \
            requires V3AXIMasterModulePort ## PORT<VT> \
        struct V3AXIMasterPortTraits<VT, PORT> { \
            static constexpr size_t DataBits \
                = sizeof(std::remove_cvref_t<decltype(std::declval<VT&>().axi_s ## PORT ## _wdata)>) * 8; \
            static constexpr size_t StrbBits \
                = sizeof(std::remove_cvref_t<decltype(std::declval<VT&>().axi_s ## PORT ## _wstrb)>) * 8; \
            static constexpr size_t RDataBits \
                = sizeof(std::remove_cvref_t<decltype(std::declval<VT&>().axi_s ## PORT ## _rdata)>) * 8; \
            \
            static_assert(DataBits == 64 || DataBits == 128 || DataBits == 256 \
                       || DataBits == 512 || DataBits == 1024, \
                "V3AXIMasterInterface: AXI master port data width must be one of 64, 128, 256, 512, 1024"); \
            static_assert(StrbBits * 8 == DataBits, \
                "V3AXIMasterInterface: AXI master port strobe width must be the data width divided by 8"); \
            static_assert(RDataBits == DataBits, \
                "V3AXIMasterInterface: AXI master port read data width must match the write data width"); \
        };

        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_TRAITS(0)
        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_TRAITS(1)
        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_TRAITS(2)
        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_TRAITS(3)
        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_TRAITS(4)
        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_TRAITS(5)
        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_TRAITS(6)
        COHESTRA_V3_DECLARE_AXI_MASTER_PORT_TRAITS(7)

#undef COHESTRA_V3_DECLARE_AXI_MASTER_PORT_TRAITS

#define COHESTRA_V3_AXI_MONITOR_PORT_REQUIREMENTS(PORT) \
        { t.mon_axi ## PORT ## _awvalid } -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _awready } -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _awid }    -> V3AXIMemoryScalarSignal; \
        { t.mon_axi ## PORT ## _awaddr }  -> V3AXIMemoryScalarSignal; \
        { t.mon_axi ## PORT ## _awlen }   -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _awsize }  -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _awburst } -> std::convertible_to<uint8_t&>; \
        \
        { t.mon_axi ## PORT ## _wvalid }  -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _wready }  -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _wdata }   -> V3AXIMemoryDataSignal; \
        { t.mon_axi ## PORT ## _wstrb }   -> V3AXIMemoryDataSignal; \
        { t.mon_axi ## PORT ## _wlast }   -> std::convertible_to<uint8_t&>; \
        \
        { t.mon_axi ## PORT ## _bvalid }  -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _bready }  -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _bid }     -> V3AXIMemoryScalarSignal; \
        { t.mon_axi ## PORT ## _bresp }   -> std::convertible_to<uint8_t&>; \
        \
        { t.mon_axi ## PORT ## _arvalid } -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _arready } -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _arid }    -> V3AXIMemoryScalarSignal; \
        { t.mon_axi ## PORT ## _araddr }  -> V3AXIMemoryScalarSignal; \
        { t.mon_axi ## PORT ## _arlen }   -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _arsize }  -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _arburst } -> std::convertible_to<uint8_t&>; \
        \
        { t.mon_axi ## PORT ## _rvalid }  -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _rready }  -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _rid }     -> V3AXIMemoryScalarSignal; \
        { t.mon_axi ## PORT ## _rdata }   -> V3AXIMemoryDataSignal; \
        { t.mon_axi ## PORT ## _rresp }   -> std::convertible_to<uint8_t&>; \
        { t.mon_axi ## PORT ## _rlast }   -> std::convertible_to<uint8_t&>;

#define COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_CONCEPTS(PORT) \
        template<class T> \
        concept V3AXIMonitorModulePort ## PORT = requires (T t) { \
            COHESTRA_V3_AXI_MONITOR_PORT_REQUIREMENTS(PORT) \
        };

        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_CONCEPTS(0)
        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_CONCEPTS(1)
        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_CONCEPTS(2)
        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_CONCEPTS(3)
        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_CONCEPTS(4)
        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_CONCEPTS(5)
        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_CONCEPTS(6)
        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_CONCEPTS(7)

#undef COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_CONCEPTS
#undef COHESTRA_V3_AXI_MONITOR_PORT_REQUIREMENTS

        // Compile-time traits of an AXI monitor port, defined only for ports
        // whose monitor taps are present on the module. Same width derivation
        // and restrictions as details::V3AXIMemoryPortTraits.
        template<class VT, size_t PORT>
        struct V3AXIMonitorPortTraits;

#define COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_TRAITS(PORT) \
        template<class VT> \
            requires V3AXIMonitorModulePort ## PORT<VT> \
        struct V3AXIMonitorPortTraits<VT, PORT> { \
            static constexpr size_t DataBits \
                = sizeof(std::remove_cvref_t<decltype(std::declval<VT&>().mon_axi ## PORT ## _wdata)>) * 8; \
            static constexpr size_t StrbBits \
                = sizeof(std::remove_cvref_t<decltype(std::declval<VT&>().mon_axi ## PORT ## _wstrb)>) * 8; \
            static constexpr size_t RDataBits \
                = sizeof(std::remove_cvref_t<decltype(std::declval<VT&>().mon_axi ## PORT ## _rdata)>) * 8; \
            \
            static_assert(DataBits == 64 || DataBits == 128 || DataBits == 256 \
                       || DataBits == 512 || DataBits == 1024, \
                "V3AXIMonitorInterface: AXI monitor port data width must be one of 64, 128, 256, 512, 1024"); \
            static_assert(StrbBits * 8 == DataBits, \
                "V3AXIMonitorInterface: AXI monitor port strobe width must be the data width divided by 8"); \
            static_assert(RDataBits == DataBits, \
                "V3AXIMonitorInterface: AXI monitor port read data width must match the write data width"); \
        };

        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_TRAITS(0)
        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_TRAITS(1)
        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_TRAITS(2)
        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_TRAITS(3)
        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_TRAITS(4)
        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_TRAITS(5)
        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_TRAITS(6)
        COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_TRAITS(7)

#undef COHESTRA_V3_DECLARE_AXI_MONITOR_PORT_TRAITS
    }


    // V3AXISlaveInterface: AXI4 memory (slave) interface of a verilated V3
    // module. Detects the 'axi_m<index>_*' pin groups of the module at compile
    // time; absent groups simply leave the port unlisted and every per-port
    // operation a no-op.
    //
    // Per port, the interface maintains a mirror bundle of the pin states of
    // the current tick. On Tick, per port:
    //  1. the Pop-asserted readys (AW/W/AR) of the last tick are cleared;
    //  2. driven B/R beats completing their handshake are retired and the
    //     completion is captured into the mirror;
    //  3. the module-driven channels (AW/W/AR) are sampled into the mirror;
    //  4. the next queued B/R beats are driven onto the pins.
    //
    // Tick is idempotent within the same timestamp, so multiple AXIMemory
    // instances sharing one interface can each tick it inside their own Tick.
    //
    // The data width of each present port is checked at compile time
    // (see details::V3AXIMemoryPortTraits) and is also available at runtime
    // through 'GetPortDataBits'.
    template<class VT>
    class V3AXISlaveInterface : public AXISlaveInterface {
    public:
        static constexpr bool   HasPort0() noexcept;
        static constexpr bool   HasPort1() noexcept;
        static constexpr bool   HasPort2() noexcept;
        static constexpr bool   HasPort3() noexcept;
        static constexpr bool   HasPort4() noexcept;
        static constexpr bool   HasPort5() noexcept;
        static constexpr bool   HasPort6() noexcept;
        static constexpr bool   HasPort7() noexcept;

        static constexpr bool   HasPort(size_t index) noexcept;

        static constexpr size_t Port0DataBits() noexcept;
        static constexpr size_t Port1DataBits() noexcept;
        static constexpr size_t Port2DataBits() noexcept;
        static constexpr size_t Port3DataBits() noexcept;
        static constexpr size_t Port4DataBits() noexcept;
        static constexpr size_t Port5DataBits() noexcept;
        static constexpr size_t Port6DataBits() noexcept;
        static constexpr size_t Port7DataBits() noexcept;

        static constexpr size_t PortDataBits(size_t index) noexcept;

    protected:
        struct Port {
            AXI::Bundle                                     bus;

            details::ResizeableFIFO<AXI::BundleChannelB>    fifoB;
            details::ResizeableFIFO<AXI::BundleChannelR>    fifoR;

            AXI::BundleChannelB                             drivenB;
            AXI::BundleChannelR                             drivenR;
            bool                                            drivenBValid    = false;
            bool                                            drivenRValid    = false;
        };

        VT*                             verilated;

        uint64_t                        lastPreTickTime     = 0;
        bool                            lastPreTickValid    = false;
        uint64_t                        lastPostTickTime    = 0;
        bool                            lastPostTickValid   = false;

        std::set<size_t>                portIndices;

        std::unordered_map<size_t, Port>
                                        ports;

    public:
        V3AXISlaveInterface(VT* verilated) noexcept;
        virtual ~V3AXISlaveInterface() noexcept = default;

    public:
        virtual void    TickPreHandshake(uint64_t time) noexcept override;
        virtual void    TickPostHandshake(uint64_t time) noexcept override;

    public:
        virtual size_t  GetPortCount() const noexcept override;

        virtual std::set<size_t>
                        GetPortIndices() const noexcept override;

        virtual size_t  GetPortMaxIndex() const noexcept override;

        virtual size_t  GetPortDataBits(size_t index) const noexcept override;

    public:
        virtual bool    HasAW(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelAW>
                        PeekAW(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelAW>
                        PopAW(size_t index) noexcept override;

        virtual bool    HasW(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelW>
                        PeekW(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelW>
                        PopW(size_t index) noexcept override;

        virtual bool    HasAR(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelAR>
                        PeekAR(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelAR>
                        PopAR(size_t index) noexcept override;

        virtual bool    PushB(size_t index, const AXI::BundleChannelB& channel) noexcept override;

        virtual bool    PushR(size_t index, const AXI::BundleChannelR& channel) noexcept override;

    public:
        bool            SetPortBFIFOSize(size_t index, size_t size = 0) noexcept;
        bool            SetPortRFIFOSize(size_t index, size_t size = 0) noexcept;
    };


    // V3AXIMasterInterface: AXI4 master (initiator) interface of a verilated
    // V3 module. Detects the 'axi_s<index>_*' pin groups of the module at
    // compile time; absent groups simply leave the port unlisted and every
    // per-port operation a no-op.
    //
    // Per port, the interface maintains a mirror bundle of the pin states of
    // the current tick. On Tick, per port:
    //  1. the Pop-asserted readys (B/R) of the last tick are cleared;
    //  2. driven AW/W/AR beats completing their handshake are retired and the
    //     completion is captured into the mirror;
    //  3. the module-driven channels (B/R) are sampled into the mirror;
    //  4. the next queued AW/W/AR beats are driven onto the pins.
    //
    // Tick is idempotent within the same timestamp, so multiple AXIMaster
    // instances sharing one interface can each tick it inside their own Tick.
    //
    // The data width of each present port is checked at compile time
    // (see details::V3AXIMasterPortTraits) and is also available at runtime
    // through 'GetPortDataBits'.
    template<class VT>
    class V3AXIMasterInterface : public AXIMasterInterface {
    public:
        static constexpr bool   HasPort0() noexcept;
        static constexpr bool   HasPort1() noexcept;
        static constexpr bool   HasPort2() noexcept;
        static constexpr bool   HasPort3() noexcept;
        static constexpr bool   HasPort4() noexcept;
        static constexpr bool   HasPort5() noexcept;
        static constexpr bool   HasPort6() noexcept;
        static constexpr bool   HasPort7() noexcept;

        static constexpr bool   HasPort(size_t index) noexcept;

        static constexpr size_t Port0DataBits() noexcept;
        static constexpr size_t Port1DataBits() noexcept;
        static constexpr size_t Port2DataBits() noexcept;
        static constexpr size_t Port3DataBits() noexcept;
        static constexpr size_t Port4DataBits() noexcept;
        static constexpr size_t Port5DataBits() noexcept;
        static constexpr size_t Port6DataBits() noexcept;
        static constexpr size_t Port7DataBits() noexcept;

        static constexpr size_t PortDataBits(size_t index) noexcept;

    protected:
        struct Port {
            AXI::Bundle                                     bus;

            details::ResizeableFIFO<AXI::BundleChannelAW>   fifoAW;
            details::ResizeableFIFO<AXI::BundleChannelW>    fifoW;
            details::ResizeableFIFO<AXI::BundleChannelAR>   fifoAR;

            AXI::BundleChannelAW                            drivenAW;
            AXI::BundleChannelW                             drivenW;
            AXI::BundleChannelAR                            drivenAR;
            bool                                            drivenAWValid   = false;
            bool                                            drivenWValid    = false;
            bool                                            drivenARValid   = false;
        };

        VT*                             verilated;

        uint64_t                        lastPreTickTime     = 0;
        bool                            lastPreTickValid    = false;
        uint64_t                        lastPostTickTime    = 0;
        bool                            lastPostTickValid   = false;

        std::set<size_t>                portIndices;

        std::unordered_map<size_t, Port>
                                        ports;

    public:
        V3AXIMasterInterface(VT* verilated) noexcept;
        virtual ~V3AXIMasterInterface() noexcept = default;

    public:
        virtual void    TickPreHandshake(uint64_t time) noexcept override;
        virtual void    TickPostHandshake(uint64_t time) noexcept override;

    public:
        virtual size_t  GetPortCount() const noexcept override;

        virtual std::set<size_t>
                        GetPortIndices() const noexcept override;

        virtual size_t  GetPortMaxIndex() const noexcept override;

        virtual size_t  GetPortDataBits(size_t index) const noexcept override;

    public:
        virtual bool    PushAW(size_t index, const AXI::BundleChannelAW& channel) noexcept override;

        virtual bool    PushW(size_t index, const AXI::BundleChannelW& channel) noexcept override;

        virtual bool    PushAR(size_t index, const AXI::BundleChannelAR& channel) noexcept override;

        virtual bool    HasB(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelB>
                        PeekB(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelB>
                        PopB(size_t index) noexcept override;

        virtual bool    HasR(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelR>
                        PeekR(size_t index) const noexcept override;
        virtual std::optional<AXI::BundleChannelR>
                        PopR(size_t index) noexcept override;

    public:
        bool            SetPortAWFIFOSize(size_t index, size_t size = 0) noexcept;
        bool            SetPortWFIFOSize(size_t index, size_t size = 0) noexcept;
        bool            SetPortARFIFOSize(size_t index, size_t size = 0) noexcept;
    };


    // V3AXIMonitorInterface: passive AXI4 monitor interface of a verilated V3
    // module. Detects the 'mon_axi<index>_*' monitor tap pin groups of the
    // module at compile time; absent groups simply leave the port unlisted
    // and every per-port operation a no-op.
    //
    // Per port, the interface maintains a mirror bundle of the tap pin states
    // of the current tick as its observation store (never exposed). On Tick,
    // per port, the valid, ready and payload of all five channels are sampled
    // into the mirror; Has reports a beat whose handshake completed on the
    // current tick (valid && ready) and Pop returns the observed time-stamped
    // payload, consuming the observation. No pins are ever driven.
    //
    // Tick is idempotent within the same timestamp, so multiple AXIMonitor
    // instances sharing one interface can each tick it inside their own Tick.
    //
    // The data width of each present port is checked at compile time
    // (see details::V3AXIMonitorPortTraits) and is also available at runtime
    // through 'GetPortDataBits'.
    template<class VT>
    class V3AXIMonitorInterface : public AXIMonitorInterface {
    public:
        static constexpr bool   HasPort0() noexcept;
        static constexpr bool   HasPort1() noexcept;
        static constexpr bool   HasPort2() noexcept;
        static constexpr bool   HasPort3() noexcept;
        static constexpr bool   HasPort4() noexcept;
        static constexpr bool   HasPort5() noexcept;
        static constexpr bool   HasPort6() noexcept;
        static constexpr bool   HasPort7() noexcept;

        static constexpr bool   HasPort(size_t index) noexcept;

        static constexpr size_t Port0DataBits() noexcept;
        static constexpr size_t Port1DataBits() noexcept;
        static constexpr size_t Port2DataBits() noexcept;
        static constexpr size_t Port3DataBits() noexcept;
        static constexpr size_t Port4DataBits() noexcept;
        static constexpr size_t Port5DataBits() noexcept;
        static constexpr size_t Port6DataBits() noexcept;
        static constexpr size_t Port7DataBits() noexcept;

        static constexpr size_t PortDataBits(size_t index) noexcept;

    protected:
        struct Port {
            AXI::Bundle     bus;
            uint64_t        time    = 0;
        };

        VT*                             verilated;

        uint64_t                        lastTickTime    = 0;
        bool                            lastTickValid   = false;

        std::set<size_t>                portIndices;

        std::unordered_map<size_t, Port>
                                        ports;

    public:
        V3AXIMonitorInterface(VT* verilated) noexcept;
        virtual ~V3AXIMonitorInterface() noexcept = default;

    public:
        virtual void    TickPostHandshake(uint64_t time) noexcept override;

    public:
        virtual size_t  GetPortCount() const noexcept override;

        virtual std::set<size_t>
                        GetPortIndices() const noexcept override;

        virtual size_t  GetPortMaxIndex() const noexcept override;

        virtual size_t  GetPortDataBits(size_t index) const noexcept override;

    public:
        virtual bool    HasAW(size_t index) const noexcept override;
        virtual std::optional<AXI::FiredChannelAW>
                        PopAW(size_t index) noexcept override;

        virtual bool    HasW(size_t index) const noexcept override;
        virtual std::optional<AXI::FiredChannelW>
                        PopW(size_t index) noexcept override;

        virtual bool    HasB(size_t index) const noexcept override;
        virtual std::optional<AXI::FiredChannelB>
                        PopB(size_t index) noexcept override;

        virtual bool    HasAR(size_t index) const noexcept override;
        virtual std::optional<AXI::FiredChannelAR>
                        PopAR(size_t index) noexcept override;

        virtual bool    HasR(size_t index) const noexcept override;
        virtual std::optional<AXI::FiredChannelR>
                        PopR(size_t index) noexcept override;
    };


    // Registers debug log listeners on all events fired by an AXIMonitor.
    void RegisterAXIMonitorListeners(
        std::shared_ptr<Cohestra::AXIMonitor> monitor, size_t index);
}


// Implementation of: class V3AXISlaveInterface
namespace Cohestra {

    template<class VT>
    inline constexpr bool V3AXISlaveInterface<VT>::HasPort0() noexcept
    {
        return details::V3AXIMemoryModulePort0<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXISlaveInterface<VT>::HasPort1() noexcept
    {
        return details::V3AXIMemoryModulePort1<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXISlaveInterface<VT>::HasPort2() noexcept
    {
        return details::V3AXIMemoryModulePort2<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXISlaveInterface<VT>::HasPort3() noexcept
    {
        return details::V3AXIMemoryModulePort3<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXISlaveInterface<VT>::HasPort4() noexcept
    {
        return details::V3AXIMemoryModulePort4<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXISlaveInterface<VT>::HasPort5() noexcept
    {
        return details::V3AXIMemoryModulePort5<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXISlaveInterface<VT>::HasPort6() noexcept
    {
        return details::V3AXIMemoryModulePort6<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXISlaveInterface<VT>::HasPort7() noexcept
    {
        return details::V3AXIMemoryModulePort7<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXISlaveInterface<VT>::HasPort(size_t index) noexcept
    {
        switch (index)
        {
            case 0: return HasPort0();
            case 1: return HasPort1();
            case 2: return HasPort2();
            case 3: return HasPort3();
            case 4: return HasPort4();
            case 5: return HasPort5();
            case 6: return HasPort6();
            case 7: return HasPort7();
            default:
                return false;
        }
    }

    template<class VT>
    inline constexpr size_t V3AXISlaveInterface<VT>::Port0DataBits() noexcept
    {
        if constexpr (HasPort0())
            return details::V3AXIMemoryPortTraits<VT, 0>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXISlaveInterface<VT>::Port1DataBits() noexcept
    {
        if constexpr (HasPort1())
            return details::V3AXIMemoryPortTraits<VT, 1>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXISlaveInterface<VT>::Port2DataBits() noexcept
    {
        if constexpr (HasPort2())
            return details::V3AXIMemoryPortTraits<VT, 2>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXISlaveInterface<VT>::Port3DataBits() noexcept
    {
        if constexpr (HasPort3())
            return details::V3AXIMemoryPortTraits<VT, 3>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXISlaveInterface<VT>::Port4DataBits() noexcept
    {
        if constexpr (HasPort4())
            return details::V3AXIMemoryPortTraits<VT, 4>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXISlaveInterface<VT>::Port5DataBits() noexcept
    {
        if constexpr (HasPort5())
            return details::V3AXIMemoryPortTraits<VT, 5>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXISlaveInterface<VT>::Port6DataBits() noexcept
    {
        if constexpr (HasPort6())
            return details::V3AXIMemoryPortTraits<VT, 6>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXISlaveInterface<VT>::Port7DataBits() noexcept
    {
        if constexpr (HasPort7())
            return details::V3AXIMemoryPortTraits<VT, 7>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXISlaveInterface<VT>::PortDataBits(size_t index) noexcept
    {
        switch (index)
        {
            case 0: return Port0DataBits();
            case 1: return Port1DataBits();
            case 2: return Port2DataBits();
            case 3: return Port3DataBits();
            case 4: return Port4DataBits();
            case 5: return Port5DataBits();
            case 6: return Port6DataBits();
            case 7: return Port7DataBits();
            default:
                return 0;
        }
    }

    template<class VT>
    inline V3AXISlaveInterface<VT>::V3AXISlaveInterface(VT* verilated) noexcept
        : verilated(verilated)
    {
#define _INIT_PORT(index) \
        if constexpr (HasPort##index()) \
        { \
            portIndices.insert(index); \
            ports[index]; \
            \
            /* forces the compile-time data width check of the port */ \
            (void)details::V3AXIMemoryPortTraits<VT, index>::DataBits; \
        }

        _INIT_PORT(0)
        _INIT_PORT(1)
        _INIT_PORT(2)
        _INIT_PORT(3)
        _INIT_PORT(4)
        _INIT_PORT(5)
        _INIT_PORT(6)
        _INIT_PORT(7)

        // TODO: support more ports

#undef _INIT_PORT
    }

    template<class VT>
    inline void V3AXISlaveInterface<VT>::TickPreHandshake(uint64_t time) noexcept
    {
        // idempotent within the same timestamp (shared by multiple memories)
        if (lastPreTickValid && time == lastPreTickTime)
            return;

        lastPreTickTime  = time;
        lastPreTickValid = true;

#define _TICK_PORT(port) \
        if constexpr (HasPort##port()) \
        { \
            details::V3AXIMemoryModulePort##port auto* axip##port = verilated; \
            \
            constexpr size_t dataLanes##port \
                = details::V3AXIMemoryPortTraits<VT, port>::DataBits / 64; \
            constexpr size_t strbLanes##port \
                = (details::V3AXIMemoryPortTraits<VT, port>::DataBits / 8 + 63) / 64; \
            \
            Port& p = ports[port]; \
            \
            /* (1) clear the Pop-asserted readys of the last tick */ \
            axip##port->axi_m##port##_awready = 0; \
            axip##port->axi_m##port##_wready  = 0; \
            axip##port->axi_m##port##_arready = 0; \
            \
            p.bus.AW.Ready = 0; \
            p.bus.W.Ready  = 0; \
            p.bus.AR.Ready = 0; \
            \
            /* (3) sample the module-driven channels into the mirror */ \
            p.bus.AW.Valid = axip##port->axi_m##port##_awvalid; \
            if (p.bus.AW.Valid) \
            { \
                p.bus.AW.Id     = axip##port->axi_m##port##_awid; \
                p.bus.AW.Addr   = axip##port->axi_m##port##_awaddr; \
                p.bus.AW.Len    = axip##port->axi_m##port##_awlen; \
                p.bus.AW.Size   = axip##port->axi_m##port##_awsize; \
                p.bus.AW.Burst  = static_cast<AXI::Burst>(axip##port->axi_m##port##_awburst); \
            } \
            \
            p.bus.W.Valid = axip##port->axi_m##port##_wvalid; \
            if (p.bus.W.Valid) \
            { \
                p.bus.W.Data.resize(dataLanes##port); \
                for (size_t i = 0; i < dataLanes##port; ++i) \
                    p.bus.W.Data[i] = details::V3AXIMemoryDataLane(axip##port->axi_m##port##_wdata, i); \
                \
                p.bus.W.Strb.resize(strbLanes##port); \
                for (size_t i = 0; i < strbLanes##port; ++i) \
                    p.bus.W.Strb[i] = details::V3AXIMemoryDataLane(axip##port->axi_m##port##_wstrb, i); \
                \
                p.bus.W.Last = axip##port->axi_m##port##_wlast; \
            } \
            \
            p.bus.AR.Valid = axip##port->axi_m##port##_arvalid; \
            if (p.bus.AR.Valid) \
            { \
                p.bus.AR.Id     = axip##port->axi_m##port##_arid; \
                p.bus.AR.Addr   = axip##port->axi_m##port##_araddr; \
                p.bus.AR.Len    = axip##port->axi_m##port##_arlen; \
                p.bus.AR.Size   = axip##port->axi_m##port##_arsize; \
                p.bus.AR.Burst  = static_cast<AXI::Burst>(axip##port->axi_m##port##_arburst); \
            } \
            \
            /* (4) drive the next queued B/R beats onto the pins */ \
            if (!p.drivenBValid && !p.fifoB.IsEmpty()) \
            { \
                p.drivenB       = *p.fifoB.Peek(); \
                p.drivenBValid  = true; \
            } \
            \
            axip##port->axi_m##port##_bvalid = p.drivenBValid ? 1 : 0; \
            if (p.drivenBValid) \
            { \
                axip##port->axi_m##port##_bid   = p.drivenB.Id; \
                axip##port->axi_m##port##_bresp = static_cast<uint8_t>(p.drivenB.Resp); \
            } \
            \
            if (!p.drivenRValid && !p.fifoR.IsEmpty()) \
            { \
                p.drivenR       = *p.fifoR.Peek(); \
                p.drivenRValid  = true; \
            } \
            \
            axip##port->axi_m##port##_rvalid = p.drivenRValid ? 1 : 0; \
            if (p.drivenRValid) \
            { \
                axip##port->axi_m##port##_rid = p.drivenR.Id; \
                \
                for (size_t i = 0; i < dataLanes##port; ++i) \
                    details::V3AXIMemoryDataSetLane(axip##port->axi_m##port##_rdata, i, \
                        i < p.drivenR.Data.size() ? p.drivenR.Data[i] : 0); \
                \
                axip##port->axi_m##port##_rresp = static_cast<uint8_t>(p.drivenR.Resp); \
                axip##port->axi_m##port##_rlast = p.drivenR.Last; \
            } \
        }

        _TICK_PORT(0)
        _TICK_PORT(1)
        _TICK_PORT(2)
        _TICK_PORT(3)
        _TICK_PORT(4)
        _TICK_PORT(5)
        _TICK_PORT(6)
        _TICK_PORT(7)

        // TODO: support more ports

#undef _TICK_PORT
    }

    template<class VT>
    inline void V3AXISlaveInterface<VT>::TickPostHandshake(uint64_t time) noexcept
    {
        // Retire phase (mid-cycle, after the clock=0 eval, before the posedge
        // eval): the settled bready/rready pins here are exactly what the
        // module registers at the upcoming posedge, so retiring the driven
        // B/R beats here agrees 1:1 with the module's capture. The pins are
        // not written in this phase; the next beat is driven at the next
        // TickPreHandshake.
        //
        // idempotent within the same timestamp (shared by multiple memories)
        if (lastPostTickValid && time == lastPostTickTime)
            return;

        lastPostTickTime  = time;
        lastPostTickValid = true;

#define _TICKPOST_PORT(port) \
        if constexpr (HasPort##port()) \
        { \
            details::V3AXIMemoryModulePort##port auto* axip##port = verilated; \
            \
            Port& p = ports[port]; \
            \
            /* (2) retire driven B/R beats completing their handshake, */ \
            /*     capturing the completion into the mirror          */ \
            p.bus.B.Ready = axip##port->axi_m##port##_bready; \
            p.bus.B.Valid = 0; \
            \
            if (p.drivenBValid && axip##port->axi_m##port##_bready) \
            { \
                p.bus.B.Valid = 1; \
                p.bus.B.Id    = p.drivenB.Id; \
                p.bus.B.Resp  = p.drivenB.Resp; \
                \
                p.fifoB.Pop(); \
                p.drivenBValid = false; \
            } \
            \
            p.bus.R.Ready = axip##port->axi_m##port##_rready; \
            p.bus.R.Valid = 0; \
            \
            if (p.drivenRValid && axip##port->axi_m##port##_rready) \
            { \
                p.bus.R.Valid = 1; \
                p.bus.R.Id    = p.drivenR.Id; \
                p.bus.R.Data  = p.drivenR.Data; \
                p.bus.R.Resp  = p.drivenR.Resp; \
                p.bus.R.Last  = p.drivenR.Last; \
                \
                p.fifoR.Pop(); \
                p.drivenRValid = false; \
            } \
        }

        _TICKPOST_PORT(0)
        _TICKPOST_PORT(1)
        _TICKPOST_PORT(2)
        _TICKPOST_PORT(3)
        _TICKPOST_PORT(4)
        _TICKPOST_PORT(5)
        _TICKPOST_PORT(6)
        _TICKPOST_PORT(7)

        // TODO: support more ports

#undef _TICKPOST_PORT
    }

    template<class VT>
    inline size_t V3AXISlaveInterface<VT>::GetPortCount() const noexcept
    {
        return portIndices.size();
    }

    template<class VT>
    inline std::set<size_t> V3AXISlaveInterface<VT>::GetPortIndices() const noexcept
    {
        return portIndices;
    }

    template<class VT>
    inline size_t V3AXISlaveInterface<VT>::GetPortMaxIndex() const noexcept
    {
        if (portIndices.empty())
            return 0;

        return *portIndices.rbegin();
    }

    template<class VT>
    inline size_t V3AXISlaveInterface<VT>::GetPortDataBits(size_t index) const noexcept
    {
        return PortDataBits(index);
    }

    template<class VT>
    inline bool V3AXISlaveInterface<VT>::HasAW(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.bus.AW.Valid != 0;
    }

    template<class VT>
    inline std::optional<AXI::BundleChannelAW> V3AXISlaveInterface<VT>::PeekAW(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end() || !iter->second.bus.AW.Valid)
            return std::nullopt;

        return { iter->second.bus.AW };
    }

    template<class VT>
    inline std::optional<AXI::BundleChannelAW> V3AXISlaveInterface<VT>::PopAW(size_t index) noexcept
    {
#define _POP_AW(port) \
        if (index == port) \
        { \
            if constexpr (HasPort##port()) \
            { \
                details::V3AXIMemoryModulePort##port auto* axip##port = verilated; \
                \
                auto& bus = ports[port].bus; \
                \
                if (bus.AW.Valid) \
                { \
                    axip##port->axi_m##port##_awready = 1; /* cleared on TickPreHandshake */ \
                    bus.AW.Ready = 1; \
                    \
                    return { bus.AW }; \
                } \
                else \
                    return std::nullopt; \
            } \
        }

        _POP_AW(0)
        _POP_AW(1)
        _POP_AW(2)
        _POP_AW(3)
        _POP_AW(4)
        _POP_AW(5)
        _POP_AW(6)
        _POP_AW(7)

        // TODO: support more ports

#undef _POP_AW

        return std::nullopt;
    }

    template<class VT>
    inline bool V3AXISlaveInterface<VT>::HasW(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.bus.W.Valid != 0;
    }

    template<class VT>
    inline std::optional<AXI::BundleChannelW> V3AXISlaveInterface<VT>::PeekW(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end() || !iter->second.bus.W.Valid)
            return std::nullopt;

        return { iter->second.bus.W };
    }

    template<class VT>
    inline std::optional<AXI::BundleChannelW> V3AXISlaveInterface<VT>::PopW(size_t index) noexcept
    {
#define _POP_W(port) \
        if (index == port) \
        { \
            if constexpr (HasPort##port()) \
            { \
                details::V3AXIMemoryModulePort##port auto* axip##port = verilated; \
                \
                auto& bus = ports[port].bus; \
                \
                if (bus.W.Valid) \
                { \
                    axip##port->axi_m##port##_wready = 1; /* cleared on TickPreHandshake */ \
                    bus.W.Ready = 1; \
                    \
                    return { bus.W }; \
                } \
                else \
                    return std::nullopt; \
            } \
        }

        _POP_W(0)
        _POP_W(1)
        _POP_W(2)
        _POP_W(3)
        _POP_W(4)
        _POP_W(5)
        _POP_W(6)
        _POP_W(7)

        // TODO: support more ports

#undef _POP_W

        return std::nullopt;
    }

    template<class VT>
    inline bool V3AXISlaveInterface<VT>::HasAR(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.bus.AR.Valid != 0;
    }

    template<class VT>
    inline std::optional<AXI::BundleChannelAR> V3AXISlaveInterface<VT>::PeekAR(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end() || !iter->second.bus.AR.Valid)
            return std::nullopt;

        return { iter->second.bus.AR };
    }

    template<class VT>
    inline std::optional<AXI::BundleChannelAR> V3AXISlaveInterface<VT>::PopAR(size_t index) noexcept
    {
#define _POP_AR(port) \
        if (index == port) \
        { \
            if constexpr (HasPort##port()) \
            { \
                details::V3AXIMemoryModulePort##port auto* axip##port = verilated; \
                \
                auto& bus = ports[port].bus; \
                \
                if (bus.AR.Valid) \
                { \
                    axip##port->axi_m##port##_arready = 1; /* cleared on TickPreHandshake */ \
                    bus.AR.Ready = 1; \
                    \
                    return { bus.AR }; \
                } \
                else \
                    return std::nullopt; \
            } \
        }

        _POP_AR(0)
        _POP_AR(1)
        _POP_AR(2)
        _POP_AR(3)
        _POP_AR(4)
        _POP_AR(5)
        _POP_AR(6)
        _POP_AR(7)

        // TODO: support more ports

#undef _POP_AR

        return std::nullopt;
    }

    template<class VT>
    inline bool V3AXISlaveInterface<VT>::PushB(size_t index, const AXI::BundleChannelB& channel) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.fifoB.Push(channel);
    }

    template<class VT>
    inline bool V3AXISlaveInterface<VT>::PushR(size_t index, const AXI::BundleChannelR& channel) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.fifoR.Push(channel);
    }

    template<class VT>
    inline bool V3AXISlaveInterface<VT>::SetPortBFIFOSize(size_t index, size_t size) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.fifoB.TryResize(size);
    }

    template<class VT>
    inline bool V3AXISlaveInterface<VT>::SetPortRFIFOSize(size_t index, size_t size) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.fifoR.TryResize(size);
    }
}


// Implementation of: class V3AXIMasterInterface
namespace Cohestra {

    template<class VT>
    inline constexpr bool V3AXIMasterInterface<VT>::HasPort0() noexcept
    {
        return details::V3AXIMasterModulePort0<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMasterInterface<VT>::HasPort1() noexcept
    {
        return details::V3AXIMasterModulePort1<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMasterInterface<VT>::HasPort2() noexcept
    {
        return details::V3AXIMasterModulePort2<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMasterInterface<VT>::HasPort3() noexcept
    {
        return details::V3AXIMasterModulePort3<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMasterInterface<VT>::HasPort4() noexcept
    {
        return details::V3AXIMasterModulePort4<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMasterInterface<VT>::HasPort5() noexcept
    {
        return details::V3AXIMasterModulePort5<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMasterInterface<VT>::HasPort6() noexcept
    {
        return details::V3AXIMasterModulePort6<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMasterInterface<VT>::HasPort7() noexcept
    {
        return details::V3AXIMasterModulePort7<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMasterInterface<VT>::HasPort(size_t index) noexcept
    {
        switch (index)
        {
            case 0: return HasPort0();
            case 1: return HasPort1();
            case 2: return HasPort2();
            case 3: return HasPort3();
            case 4: return HasPort4();
            case 5: return HasPort5();
            case 6: return HasPort6();
            case 7: return HasPort7();
            default:
                return false;
        }
    }

    template<class VT>
    inline constexpr size_t V3AXIMasterInterface<VT>::Port0DataBits() noexcept
    {
        if constexpr (HasPort0())
            return details::V3AXIMasterPortTraits<VT, 0>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMasterInterface<VT>::Port1DataBits() noexcept
    {
        if constexpr (HasPort1())
            return details::V3AXIMasterPortTraits<VT, 1>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMasterInterface<VT>::Port2DataBits() noexcept
    {
        if constexpr (HasPort2())
            return details::V3AXIMasterPortTraits<VT, 2>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMasterInterface<VT>::Port3DataBits() noexcept
    {
        if constexpr (HasPort3())
            return details::V3AXIMasterPortTraits<VT, 3>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMasterInterface<VT>::Port4DataBits() noexcept
    {
        if constexpr (HasPort4())
            return details::V3AXIMasterPortTraits<VT, 4>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMasterInterface<VT>::Port5DataBits() noexcept
    {
        if constexpr (HasPort5())
            return details::V3AXIMasterPortTraits<VT, 5>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMasterInterface<VT>::Port6DataBits() noexcept
    {
        if constexpr (HasPort6())
            return details::V3AXIMasterPortTraits<VT, 6>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMasterInterface<VT>::Port7DataBits() noexcept
    {
        if constexpr (HasPort7())
            return details::V3AXIMasterPortTraits<VT, 7>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMasterInterface<VT>::PortDataBits(size_t index) noexcept
    {
        switch (index)
        {
            case 0: return Port0DataBits();
            case 1: return Port1DataBits();
            case 2: return Port2DataBits();
            case 3: return Port3DataBits();
            case 4: return Port4DataBits();
            case 5: return Port5DataBits();
            case 6: return Port6DataBits();
            case 7: return Port7DataBits();
            default:
                return 0;
        }
    }

    template<class VT>
    inline V3AXIMasterInterface<VT>::V3AXIMasterInterface(VT* verilated) noexcept
        : verilated(verilated)
    {
#define _INIT_PORT(index) \
        if constexpr (HasPort##index()) \
        { \
            portIndices.insert(index); \
            ports[index]; \
            \
            /* forces the compile-time data width check of the port */ \
            (void)details::V3AXIMasterPortTraits<VT, index>::DataBits; \
        }

        _INIT_PORT(0)
        _INIT_PORT(1)
        _INIT_PORT(2)
        _INIT_PORT(3)
        _INIT_PORT(4)
        _INIT_PORT(5)
        _INIT_PORT(6)
        _INIT_PORT(7)

        // TODO: support more ports

#undef _INIT_PORT
    }

    template<class VT>
    inline void V3AXIMasterInterface<VT>::TickPreHandshake(uint64_t time) noexcept
    {
        // idempotent within the same timestamp (shared by multiple masters)
        if (lastPreTickValid && time == lastPreTickTime)
            return;

        lastPreTickTime  = time;
        lastPreTickValid = true;

#define _TICK_PORT(port) \
        if constexpr (HasPort##port()) \
        { \
            details::V3AXIMasterModulePort##port auto* axip##port = verilated; \
            \
            constexpr size_t dataLanes##port \
                = details::V3AXIMasterPortTraits<VT, port>::DataBits / 64; \
            constexpr size_t strbLanes##port \
                = (details::V3AXIMasterPortTraits<VT, port>::DataBits / 8 + 63) / 64; \
            \
            Port& p = ports[port]; \
            \
            /* (1) clear the Pop-asserted readys of the last tick */ \
            axip##port->axi_s##port##_bready = 0; \
            axip##port->axi_s##port##_rready = 0; \
            \
            p.bus.B.Ready = 0; \
            p.bus.R.Ready = 0; \
            \
            /* (3) sample the module-driven channels into the mirror */ \
            p.bus.B.Valid = axip##port->axi_s##port##_bvalid; \
            if (p.bus.B.Valid) \
            { \
                p.bus.B.Id    = axip##port->axi_s##port##_bid; \
                p.bus.B.Resp  = static_cast<AXI::Resp>(axip##port->axi_s##port##_bresp); \
            } \
            \
            p.bus.R.Valid = axip##port->axi_s##port##_rvalid; \
            if (p.bus.R.Valid) \
            { \
                p.bus.R.Id = axip##port->axi_s##port##_rid; \
                \
                p.bus.R.Data.resize(dataLanes##port); \
                for (size_t i = 0; i < dataLanes##port; ++i) \
                    p.bus.R.Data[i] = details::V3AXIMemoryDataLane(axip##port->axi_s##port##_rdata, i); \
                \
                p.bus.R.Resp  = static_cast<AXI::Resp>(axip##port->axi_s##port##_rresp); \
                p.bus.R.Last  = axip##port->axi_s##port##_rlast; \
            } \
            \
            /* (4) drive the next queued AW/W/AR beats onto the pins */ \
            if (!p.drivenAWValid && !p.fifoAW.IsEmpty()) \
            { \
                p.drivenAW      = *p.fifoAW.Peek(); \
                p.drivenAWValid = true; \
            } \
            \
            axip##port->axi_s##port##_awvalid = p.drivenAWValid ? 1 : 0; \
            if (p.drivenAWValid) \
            { \
                axip##port->axi_s##port##_awid    = p.drivenAW.Id; \
                axip##port->axi_s##port##_awaddr  = p.drivenAW.Addr; \
                axip##port->axi_s##port##_awlen   = p.drivenAW.Len; \
                axip##port->axi_s##port##_awsize  = p.drivenAW.Size; \
                axip##port->axi_s##port##_awburst = static_cast<uint8_t>(p.drivenAW.Burst); \
            } \
            \
            if (!p.drivenWValid && !p.fifoW.IsEmpty()) \
            { \
                p.drivenW       = *p.fifoW.Peek(); \
                p.drivenWValid  = true; \
            } \
            \
            axip##port->axi_s##port##_wvalid = p.drivenWValid ? 1 : 0; \
            if (p.drivenWValid) \
            { \
                for (size_t i = 0; i < dataLanes##port; ++i) \
                    details::V3AXIMemoryDataSetLane(axip##port->axi_s##port##_wdata, i, \
                        i < p.drivenW.Data.size() ? p.drivenW.Data[i] : 0); \
                \
                for (size_t i = 0; i < strbLanes##port; ++i) \
                    details::V3AXIMemoryDataSetLane(axip##port->axi_s##port##_wstrb, i, \
                        i < p.drivenW.Strb.size() ? p.drivenW.Strb[i] : 0); \
                \
                axip##port->axi_s##port##_wlast = p.drivenW.Last; \
            } \
            \
            if (!p.drivenARValid && !p.fifoAR.IsEmpty()) \
            { \
                p.drivenAR      = *p.fifoAR.Peek(); \
                p.drivenARValid = true; \
            } \
            \
            axip##port->axi_s##port##_arvalid = p.drivenARValid ? 1 : 0; \
            if (p.drivenARValid) \
            { \
                axip##port->axi_s##port##_arid    = p.drivenAR.Id; \
                axip##port->axi_s##port##_araddr  = p.drivenAR.Addr; \
                axip##port->axi_s##port##_arlen   = p.drivenAR.Len; \
                axip##port->axi_s##port##_arsize  = p.drivenAR.Size; \
                axip##port->axi_s##port##_arburst = static_cast<uint8_t>(p.drivenAR.Burst); \
            } \
        }

        _TICK_PORT(0)
        _TICK_PORT(1)
        _TICK_PORT(2)
        _TICK_PORT(3)
        _TICK_PORT(4)
        _TICK_PORT(5)
        _TICK_PORT(6)
        _TICK_PORT(7)

        // TODO: support more ports

#undef _TICK_PORT
    }

    template<class VT>
    inline void V3AXIMasterInterface<VT>::TickPostHandshake(uint64_t time) noexcept
    {
        // Retire phase (mid-cycle, after the clock=0 eval, before the posedge
        // eval): the settled awready/wready/arready pins here are exactly what
        // the module registers at the upcoming posedge, so retiring the driven
        // AW/W/AR beats here agrees 1:1 with the module's capture. The pins are
        // not written in this phase; the next beat is driven at the next
        // TickPreHandshake.
        //
        // idempotent within the same timestamp (shared by multiple masters)
        if (lastPostTickValid && time == lastPostTickTime)
            return;

        lastPostTickTime  = time;
        lastPostTickValid = true;

#define _TICKPOST_PORT(port) \
        if constexpr (HasPort##port()) \
        { \
            details::V3AXIMasterModulePort##port auto* axip##port = verilated; \
            \
            Port& p = ports[port]; \
            \
            /* (2) retire driven AW/W/AR beats completing their handshake, */ \
            /*     capturing the completion into the mirror                 */ \
            p.bus.AW.Ready = axip##port->axi_s##port##_awready; \
            p.bus.AW.Valid = 0; \
            \
            if (p.drivenAWValid && axip##port->axi_s##port##_awready) \
            { \
                p.bus.AW.Valid  = 1; \
                p.bus.AW.Id     = p.drivenAW.Id; \
                p.bus.AW.Addr   = p.drivenAW.Addr; \
                p.bus.AW.Len    = p.drivenAW.Len; \
                p.bus.AW.Size   = p.drivenAW.Size; \
                p.bus.AW.Burst  = p.drivenAW.Burst; \
                \
                p.fifoAW.Pop(); \
                p.drivenAWValid = false; \
            } \
            \
            p.bus.W.Ready = axip##port->axi_s##port##_wready; \
            p.bus.W.Valid = 0; \
            \
            if (p.drivenWValid && axip##port->axi_s##port##_wready) \
            { \
                p.bus.W.Valid = 1; \
                p.bus.W.Data  = p.drivenW.Data; \
                p.bus.W.Strb  = p.drivenW.Strb; \
                p.bus.W.Last  = p.drivenW.Last; \
                \
                p.fifoW.Pop(); \
                p.drivenWValid = false; \
            } \
            \
            p.bus.AR.Ready = axip##port->axi_s##port##_arready; \
            p.bus.AR.Valid = 0; \
            \
            if (p.drivenARValid && axip##port->axi_s##port##_arready) \
            { \
                p.bus.AR.Valid  = 1; \
                p.bus.AR.Id     = p.drivenAR.Id; \
                p.bus.AR.Addr   = p.drivenAR.Addr; \
                p.bus.AR.Len    = p.drivenAR.Len; \
                p.bus.AR.Size   = p.drivenAR.Size; \
                p.bus.AR.Burst  = p.drivenAR.Burst; \
                \
                p.fifoAR.Pop(); \
                p.drivenARValid = false; \
            } \
        }

        _TICKPOST_PORT(0)
        _TICKPOST_PORT(1)
        _TICKPOST_PORT(2)
        _TICKPOST_PORT(3)
        _TICKPOST_PORT(4)
        _TICKPOST_PORT(5)
        _TICKPOST_PORT(6)
        _TICKPOST_PORT(7)

        // TODO: support more ports

#undef _TICKPOST_PORT
    }

    template<class VT>
    inline size_t V3AXIMasterInterface<VT>::GetPortCount() const noexcept
    {
        return portIndices.size();
    }

    template<class VT>
    inline std::set<size_t> V3AXIMasterInterface<VT>::GetPortIndices() const noexcept
    {
        return portIndices;
    }

    template<class VT>
    inline size_t V3AXIMasterInterface<VT>::GetPortMaxIndex() const noexcept
    {
        if (portIndices.empty())
            return 0;

        return *portIndices.rbegin();
    }

    template<class VT>
    inline size_t V3AXIMasterInterface<VT>::GetPortDataBits(size_t index) const noexcept
    {
        return PortDataBits(index);
    }

    template<class VT>
    inline bool V3AXIMasterInterface<VT>::PushAW(size_t index, const AXI::BundleChannelAW& channel) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.fifoAW.Push(channel);
    }

    template<class VT>
    inline bool V3AXIMasterInterface<VT>::PushW(size_t index, const AXI::BundleChannelW& channel) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.fifoW.Push(channel);
    }

    template<class VT>
    inline bool V3AXIMasterInterface<VT>::PushAR(size_t index, const AXI::BundleChannelAR& channel) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.fifoAR.Push(channel);
    }

    template<class VT>
    inline bool V3AXIMasterInterface<VT>::HasB(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.bus.B.Valid != 0;
    }

    template<class VT>
    inline std::optional<AXI::BundleChannelB> V3AXIMasterInterface<VT>::PeekB(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end() || !iter->second.bus.B.Valid)
            return std::nullopt;

        return { iter->second.bus.B };
    }

    template<class VT>
    inline std::optional<AXI::BundleChannelB> V3AXIMasterInterface<VT>::PopB(size_t index) noexcept
    {
#define _POP_B(port) \
        if (index == port) \
        { \
            if constexpr (HasPort##port()) \
            { \
                details::V3AXIMasterModulePort##port auto* axip##port = verilated; \
                \
                auto& bus = ports[port].bus; \
                \
                if (bus.B.Valid) \
                { \
                    axip##port->axi_s##port##_bready = 1; /* cleared on TickPreHandshake */ \
                    bus.B.Ready = 1; \
                    \
                    return { bus.B }; \
                } \
                else \
                    return std::nullopt; \
            } \
        }

        _POP_B(0)
        _POP_B(1)
        _POP_B(2)
        _POP_B(3)
        _POP_B(4)
        _POP_B(5)
        _POP_B(6)
        _POP_B(7)

        // TODO: support more ports

#undef _POP_B

        return std::nullopt;
    }

    template<class VT>
    inline bool V3AXIMasterInterface<VT>::HasR(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.bus.R.Valid != 0;
    }

    template<class VT>
    inline std::optional<AXI::BundleChannelR> V3AXIMasterInterface<VT>::PeekR(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end() || !iter->second.bus.R.Valid)
            return std::nullopt;

        return { iter->second.bus.R };
    }

    template<class VT>
    inline std::optional<AXI::BundleChannelR> V3AXIMasterInterface<VT>::PopR(size_t index) noexcept
    {
#define _POP_R(port) \
        if (index == port) \
        { \
            if constexpr (HasPort##port()) \
            { \
                details::V3AXIMasterModulePort##port auto* axip##port = verilated; \
                \
                auto& bus = ports[port].bus; \
                \
                if (bus.R.Valid) \
                { \
                    axip##port->axi_s##port##_rready = 1; /* cleared on TickPreHandshake */ \
                    bus.R.Ready = 1; \
                    \
                    return { bus.R }; \
                } \
                else \
                    return std::nullopt; \
            } \
        }

        _POP_R(0)
        _POP_R(1)
        _POP_R(2)
        _POP_R(3)
        _POP_R(4)
        _POP_R(5)
        _POP_R(6)
        _POP_R(7)

        // TODO: support more ports

#undef _POP_R

        return std::nullopt;
    }

    template<class VT>
    inline bool V3AXIMasterInterface<VT>::SetPortAWFIFOSize(size_t index, size_t size) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.fifoAW.TryResize(size);
    }

    template<class VT>
    inline bool V3AXIMasterInterface<VT>::SetPortWFIFOSize(size_t index, size_t size) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.fifoW.TryResize(size);
    }

    template<class VT>
    inline bool V3AXIMasterInterface<VT>::SetPortARFIFOSize(size_t index, size_t size) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.fifoAR.TryResize(size);
    }
}


// Implementation of: class V3AXIMonitorInterface
namespace Cohestra {

    template<class VT>
    inline constexpr bool V3AXIMonitorInterface<VT>::HasPort0() noexcept
    {
        return details::V3AXIMonitorModulePort0<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMonitorInterface<VT>::HasPort1() noexcept
    {
        return details::V3AXIMonitorModulePort1<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMonitorInterface<VT>::HasPort2() noexcept
    {
        return details::V3AXIMonitorModulePort2<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMonitorInterface<VT>::HasPort3() noexcept
    {
        return details::V3AXIMonitorModulePort3<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMonitorInterface<VT>::HasPort4() noexcept
    {
        return details::V3AXIMonitorModulePort4<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMonitorInterface<VT>::HasPort5() noexcept
    {
        return details::V3AXIMonitorModulePort5<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMonitorInterface<VT>::HasPort6() noexcept
    {
        return details::V3AXIMonitorModulePort6<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMonitorInterface<VT>::HasPort7() noexcept
    {
        return details::V3AXIMonitorModulePort7<VT>;
    }

    template<class VT>
    inline constexpr bool V3AXIMonitorInterface<VT>::HasPort(size_t index) noexcept
    {
        switch (index)
        {
            case 0: return HasPort0();
            case 1: return HasPort1();
            case 2: return HasPort2();
            case 3: return HasPort3();
            case 4: return HasPort4();
            case 5: return HasPort5();
            case 6: return HasPort6();
            case 7: return HasPort7();
            default:
                return false;
        }
    }

    template<class VT>
    inline constexpr size_t V3AXIMonitorInterface<VT>::Port0DataBits() noexcept
    {
        if constexpr (HasPort0())
            return details::V3AXIMonitorPortTraits<VT, 0>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMonitorInterface<VT>::Port1DataBits() noexcept
    {
        if constexpr (HasPort1())
            return details::V3AXIMonitorPortTraits<VT, 1>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMonitorInterface<VT>::Port2DataBits() noexcept
    {
        if constexpr (HasPort2())
            return details::V3AXIMonitorPortTraits<VT, 2>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMonitorInterface<VT>::Port3DataBits() noexcept
    {
        if constexpr (HasPort3())
            return details::V3AXIMonitorPortTraits<VT, 3>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMonitorInterface<VT>::Port4DataBits() noexcept
    {
        if constexpr (HasPort4())
            return details::V3AXIMonitorPortTraits<VT, 4>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMonitorInterface<VT>::Port5DataBits() noexcept
    {
        if constexpr (HasPort5())
            return details::V3AXIMonitorPortTraits<VT, 5>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMonitorInterface<VT>::Port6DataBits() noexcept
    {
        if constexpr (HasPort6())
            return details::V3AXIMonitorPortTraits<VT, 6>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMonitorInterface<VT>::Port7DataBits() noexcept
    {
        if constexpr (HasPort7())
            return details::V3AXIMonitorPortTraits<VT, 7>::DataBits;
        else
            return 0;
    }

    template<class VT>
    inline constexpr size_t V3AXIMonitorInterface<VT>::PortDataBits(size_t index) noexcept
    {
        switch (index)
        {
            case 0: return Port0DataBits();
            case 1: return Port1DataBits();
            case 2: return Port2DataBits();
            case 3: return Port3DataBits();
            case 4: return Port4DataBits();
            case 5: return Port5DataBits();
            case 6: return Port6DataBits();
            case 7: return Port7DataBits();
            default:
                return 0;
        }
    }

    template<class VT>
    inline V3AXIMonitorInterface<VT>::V3AXIMonitorInterface(VT* verilated) noexcept
        : verilated(verilated)
    {
#define _INIT_PORT(index) \
        if constexpr (HasPort##index()) \
        { \
            portIndices.insert(index); \
            ports[index]; \
            \
            /* forces the compile-time data width check of the port */ \
            (void)details::V3AXIMonitorPortTraits<VT, index>::DataBits; \
        }

        _INIT_PORT(0)
        _INIT_PORT(1)
        _INIT_PORT(2)
        _INIT_PORT(3)
        _INIT_PORT(4)
        _INIT_PORT(5)
        _INIT_PORT(6)
        _INIT_PORT(7)

        // TODO: support more ports

#undef _INIT_PORT
    }

    template<class VT>
    inline void V3AXIMonitorInterface<VT>::TickPostHandshake(uint64_t time) noexcept
    {
        // idempotent within the same timestamp (shared by multiple monitors)
        if (lastTickValid && time == lastTickTime)
            return;

        lastTickTime  = time;
        lastTickValid = true;

#define _TICK_PORT(port) \
        if constexpr (HasPort##port()) \
        { \
            details::V3AXIMonitorModulePort##port auto* monp##port = verilated; \
            \
            constexpr size_t dataLanes##port \
                = details::V3AXIMonitorPortTraits<VT, port>::DataBits / 64; \
            constexpr size_t strbLanes##port \
                = (details::V3AXIMonitorPortTraits<VT, port>::DataBits / 8 + 63) / 64; \
            \
            Port& p = ports[port]; \
            \
            p.time = time; \
            \
            /* sample the AW channel */ \
            p.bus.AW.Valid = monp##port->mon_axi##port##_awvalid; \
            p.bus.AW.Ready = monp##port->mon_axi##port##_awready; \
            if (p.bus.AW.Valid) \
            { \
                p.bus.AW.Id     = monp##port->mon_axi##port##_awid; \
                p.bus.AW.Addr   = monp##port->mon_axi##port##_awaddr; \
                p.bus.AW.Len    = monp##port->mon_axi##port##_awlen; \
                p.bus.AW.Size   = monp##port->mon_axi##port##_awsize; \
                p.bus.AW.Burst  = static_cast<AXI::Burst>(monp##port->mon_axi##port##_awburst); \
            } \
            \
            /* sample the W channel */ \
            p.bus.W.Valid = monp##port->mon_axi##port##_wvalid; \
            p.bus.W.Ready = monp##port->mon_axi##port##_wready; \
            if (p.bus.W.Valid) \
            { \
                p.bus.W.Data.resize(dataLanes##port); \
                for (size_t i = 0; i < dataLanes##port; ++i) \
                    p.bus.W.Data[i] = details::V3AXIMemoryDataLane(monp##port->mon_axi##port##_wdata, i); \
                \
                p.bus.W.Strb.resize(strbLanes##port); \
                for (size_t i = 0; i < strbLanes##port; ++i) \
                    p.bus.W.Strb[i] = details::V3AXIMemoryDataLane(monp##port->mon_axi##port##_wstrb, i); \
                \
                p.bus.W.Last = monp##port->mon_axi##port##_wlast; \
            } \
            \
            /* sample the B channel */ \
            p.bus.B.Valid = monp##port->mon_axi##port##_bvalid; \
            p.bus.B.Ready = monp##port->mon_axi##port##_bready; \
            if (p.bus.B.Valid) \
            { \
                p.bus.B.Id    = monp##port->mon_axi##port##_bid; \
                p.bus.B.Resp  = static_cast<AXI::Resp>(monp##port->mon_axi##port##_bresp); \
            } \
            \
            /* sample the AR channel */ \
            p.bus.AR.Valid = monp##port->mon_axi##port##_arvalid; \
            p.bus.AR.Ready = monp##port->mon_axi##port##_arready; \
            if (p.bus.AR.Valid) \
            { \
                p.bus.AR.Id     = monp##port->mon_axi##port##_arid; \
                p.bus.AR.Addr   = monp##port->mon_axi##port##_araddr; \
                p.bus.AR.Len    = monp##port->mon_axi##port##_arlen; \
                p.bus.AR.Size   = monp##port->mon_axi##port##_arsize; \
                p.bus.AR.Burst  = static_cast<AXI::Burst>(monp##port->mon_axi##port##_arburst); \
            } \
            \
            /* sample the R channel */ \
            p.bus.R.Valid = monp##port->mon_axi##port##_rvalid; \
            p.bus.R.Ready = monp##port->mon_axi##port##_rready; \
            if (p.bus.R.Valid) \
            { \
                p.bus.R.Id = monp##port->mon_axi##port##_rid; \
                \
                p.bus.R.Data.resize(dataLanes##port); \
                for (size_t i = 0; i < dataLanes##port; ++i) \
                    p.bus.R.Data[i] = details::V3AXIMemoryDataLane(monp##port->mon_axi##port##_rdata, i); \
                \
                p.bus.R.Resp  = static_cast<AXI::Resp>(monp##port->mon_axi##port##_rresp); \
                p.bus.R.Last  = monp##port->mon_axi##port##_rlast; \
            } \
        }

        _TICK_PORT(0)
        _TICK_PORT(1)
        _TICK_PORT(2)
        _TICK_PORT(3)
        _TICK_PORT(4)
        _TICK_PORT(5)
        _TICK_PORT(6)
        _TICK_PORT(7)

        // TODO: support more ports

#undef _TICK_PORT
    }

    template<class VT>
    inline size_t V3AXIMonitorInterface<VT>::GetPortCount() const noexcept
    {
        return portIndices.size();
    }

    template<class VT>
    inline std::set<size_t> V3AXIMonitorInterface<VT>::GetPortIndices() const noexcept
    {
        return portIndices;
    }

    template<class VT>
    inline size_t V3AXIMonitorInterface<VT>::GetPortMaxIndex() const noexcept
    {
        if (portIndices.empty())
            return 0;

        return *portIndices.rbegin();
    }

    template<class VT>
    inline size_t V3AXIMonitorInterface<VT>::GetPortDataBits(size_t index) const noexcept
    {
        return PortDataBits(index);
    }

    template<class VT>
    inline bool V3AXIMonitorInterface<VT>::HasAW(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.bus.AW.Fire();
    }

    template<class VT>
    inline std::optional<AXI::FiredChannelAW> V3AXIMonitorInterface<VT>::PopAW(size_t index) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end() || !iter->second.bus.AW.Fire())
            return std::nullopt;

        // consume the observation
        iter->second.bus.AW.Valid = 0;

        return { { iter->second.time, iter->second.bus.AW } };
    }

    template<class VT>
    inline bool V3AXIMonitorInterface<VT>::HasW(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.bus.W.Fire();
    }

    template<class VT>
    inline std::optional<AXI::FiredChannelW> V3AXIMonitorInterface<VT>::PopW(size_t index) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end() || !iter->second.bus.W.Fire())
            return std::nullopt;

        // consume the observation
        iter->second.bus.W.Valid = 0;

        return { { iter->second.time, iter->second.bus.W } };
    }

    template<class VT>
    inline bool V3AXIMonitorInterface<VT>::HasB(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.bus.B.Fire();
    }

    template<class VT>
    inline std::optional<AXI::FiredChannelB> V3AXIMonitorInterface<VT>::PopB(size_t index) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end() || !iter->second.bus.B.Fire())
            return std::nullopt;

        // consume the observation
        iter->second.bus.B.Valid = 0;

        return { { iter->second.time, iter->second.bus.B } };
    }

    template<class VT>
    inline bool V3AXIMonitorInterface<VT>::HasAR(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.bus.AR.Fire();
    }

    template<class VT>
    inline std::optional<AXI::FiredChannelAR> V3AXIMonitorInterface<VT>::PopAR(size_t index) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end() || !iter->second.bus.AR.Fire())
            return std::nullopt;

        // consume the observation
        iter->second.bus.AR.Valid = 0;

        return { { iter->second.time, iter->second.bus.AR } };
    }

    template<class VT>
    inline bool V3AXIMonitorInterface<VT>::HasR(size_t index) const noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end())
            return false;

        return iter->second.bus.R.Fire();
    }

    template<class VT>
    inline std::optional<AXI::FiredChannelR> V3AXIMonitorInterface<VT>::PopR(size_t index) noexcept
    {
        auto iter = ports.find(index);
        if (iter == ports.end() || !iter->second.bus.R.Fire())
            return std::nullopt;

        // consume the observation
        iter->second.bus.R.Valid = 0;

        return { { iter->second.time, iter->second.bus.R } };
    }
}
