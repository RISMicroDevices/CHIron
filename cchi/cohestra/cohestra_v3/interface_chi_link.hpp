#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>

namespace Cohestra {
namespace details {

#define COHESTRA_V3_CHI_LINK_REQUIREMENTS(PORT) \
    { t.chi_rn ## PORT ## _txlinkactivereq } -> std::convertible_to<uint8_t&>; \
    { t.chi_rn ## PORT ## _txlinkactiveack } -> std::convertible_to<uint8_t&>; \
    { t.chi_rn ## PORT ## _rxlinkactivereq } -> std::convertible_to<uint8_t&>; \
    { t.chi_rn ## PORT ## _rxlinkactiveack } -> std::convertible_to<uint8_t&>; \
    { t.chi_rn ## PORT ## _rxsactive } -> std::convertible_to<uint8_t&>; \
    { t.chi_rn ## PORT ## _syscoreq } -> std::convertible_to<uint8_t&>; \
    { t.chi_rn ## PORT ## _syscoack } -> std::convertible_to<uint8_t&>

#define COHESTRA_V3_DECLARE_CHI_LINK_CONCEPT(PORT) \
    template<class T> \
    concept V3CHIModuleLinkPort ## PORT = requires (T t) { \
        COHESTRA_V3_CHI_LINK_REQUIREMENTS(PORT); \
    }

        COHESTRA_V3_DECLARE_CHI_LINK_CONCEPT(0);
        COHESTRA_V3_DECLARE_CHI_LINK_CONCEPT(1);
        COHESTRA_V3_DECLARE_CHI_LINK_CONCEPT(2);
        COHESTRA_V3_DECLARE_CHI_LINK_CONCEPT(3);
        COHESTRA_V3_DECLARE_CHI_LINK_CONCEPT(4);
        COHESTRA_V3_DECLARE_CHI_LINK_CONCEPT(5);
        COHESTRA_V3_DECLARE_CHI_LINK_CONCEPT(6);
        COHESTRA_V3_DECLARE_CHI_LINK_CONCEPT(7);

#undef COHESTRA_V3_DECLARE_CHI_LINK_CONCEPT
#undef COHESTRA_V3_CHI_LINK_REQUIREMENTS

        template<class VT, size_t PORT>
        struct V3CHILinkPinTraits;

#define V3_CHI_DEFINE_LINK_TRAITS(PORT) \
        template<class VT> \
        struct V3CHILinkPinTraits<VT, PORT> { \
            static uint8_t& TxReq(VT& v) noexcept { return v.chi_rn ## PORT ## _txlinkactivereq; } \
            static uint8_t& TxAck(VT& v) noexcept { return v.chi_rn ## PORT ## _txlinkactiveack; } \
            static uint8_t& RxReq(VT& v) noexcept { return v.chi_rn ## PORT ## _rxlinkactivereq; } \
            static uint8_t& RxAck(VT& v) noexcept { return v.chi_rn ## PORT ## _rxlinkactiveack; } \
            static uint8_t& RxSActive(VT& v) noexcept { return v.chi_rn ## PORT ## _rxsactive; } \
            static uint8_t& SyscoReq(VT& v) noexcept { return v.chi_rn ## PORT ## _syscoreq; } \
            static uint8_t& SyscoAck(VT& v) noexcept { return v.chi_rn ## PORT ## _syscoack; } \
        }

        V3_CHI_DEFINE_LINK_TRAITS(0);
        V3_CHI_DEFINE_LINK_TRAITS(1);
        V3_CHI_DEFINE_LINK_TRAITS(2);
        V3_CHI_DEFINE_LINK_TRAITS(3);
        V3_CHI_DEFINE_LINK_TRAITS(4);
        V3_CHI_DEFINE_LINK_TRAITS(5);
        V3_CHI_DEFINE_LINK_TRAITS(6);
        V3_CHI_DEFINE_LINK_TRAITS(7);

#undef V3_CHI_DEFINE_LINK_TRAITS

}

    template<class VT>
    class V3CHILinkControl {
    private:
        VT*     verilated;
        bool    active = false;

    public:
        static constexpr bool HasPort0() noexcept { return details::V3CHIModuleLinkPort0<VT>; }
        static constexpr bool HasPort1() noexcept { return details::V3CHIModuleLinkPort1<VT>; }
        static constexpr bool HasPort2() noexcept { return details::V3CHIModuleLinkPort2<VT>; }
        static constexpr bool HasPort3() noexcept { return details::V3CHIModuleLinkPort3<VT>; }
        static constexpr bool HasPort4() noexcept { return details::V3CHIModuleLinkPort4<VT>; }
        static constexpr bool HasPort5() noexcept { return details::V3CHIModuleLinkPort5<VT>; }
        static constexpr bool HasPort6() noexcept { return details::V3CHIModuleLinkPort6<VT>; }
        static constexpr bool HasPort7() noexcept { return details::V3CHIModuleLinkPort7<VT>; }

        static constexpr bool HasPort(size_t index) noexcept
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
                default: return false;
            }
        }

        explicit V3CHILinkControl(VT* verilated) noexcept : verilated(verilated) { }

        void Reset() noexcept
        {
            active = false;
            TickPreHandshake();
        }

        void SetActive(bool value) noexcept { active = value; }

        void TickPreHandshake() noexcept
        {
#define _TICK_LINK(port) \
            if constexpr (HasPort##port()) \
            { \
                using Traits = details::V3CHILinkPinTraits<VT, port>; \
                Traits::RxReq(*verilated) = active ? 1 : 0; \
                Traits::TxAck(*verilated) = active && Traits::TxReq(*verilated) ? 1 : 0; \
                Traits::RxSActive(*verilated) = active ? 1 : 0; \
                Traits::SyscoAck(*verilated) = active ? 1 : 0; \
            }
            _TICK_LINK(0)
            _TICK_LINK(1)
            _TICK_LINK(2)
            _TICK_LINK(3)
            _TICK_LINK(4)
            _TICK_LINK(5)
            _TICK_LINK(6)
            _TICK_LINK(7)
#undef _TICK_LINK
        }

        bool IsRun(size_t index) const noexcept
        {
#define _IS_RUN(port) \
            if (index == port) \
            { \
                if constexpr (HasPort##port()) \
                { \
                    using Traits = details::V3CHILinkPinTraits<VT, port>; \
                    return active && Traits::TxReq(*verilated) && Traits::RxAck(*verilated); \
                } \
                else \
                    return false; \
            }
            _IS_RUN(0)
            _IS_RUN(1)
            _IS_RUN(2)
            _IS_RUN(3)
            _IS_RUN(4)
            _IS_RUN(5)
            _IS_RUN(6)
            _IS_RUN(7)
#undef _IS_RUN
            return false;
        }
    };

}
