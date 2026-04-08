//#pragma once

#include "includes.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_B__SNP_DETAILS)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_EB__SNP_DETAILS))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_B__SNP_DETAILS
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_EB__SNP_DETAILS
#endif


/*
namespace CHI {
*/
    namespace Flits {
        
        class SNPBuildTargetEnumBack {
        public:
            const char*                     name;
            const int                       opcode;
            const Xact::SnoopFieldMapping   fields;

        public:
            inline constexpr SNPBuildTargetEnumBack(const char* name, const int opcode, const Xact::SnoopFieldMapping fields) noexcept
            : name(name), opcode(opcode), fields(fields) { }

        public:
            inline constexpr operator int() const noexcept
            { return opcode; }

            inline constexpr operator const SNPBuildTargetEnumBack*() const noexcept
            { return this; }

            inline constexpr bool operator==(const SNPBuildTargetEnumBack& obj) const noexcept
            { return opcode == obj.opcode; }

            inline constexpr bool operator!=(const SNPBuildTargetEnumBack& obj) const noexcept
            { return !(*this == obj); }
        };

        using SNPBuildTargetEnum = const SNPBuildTargetEnumBack*;
    }

    namespace Flits::details {

        enum class SNPFlitField {
            QoS,
            SrcID,
            TxnID,
            FwdNID,
            FwdTxnID,
            StashLPID,
            StashLPIDValid,
            VMIDExt,
            Opcode,
            Addr,
            NS,
            DoNotGoToSD,
#if defined(CHI_ISSUE_B_ENABLE) || defined(CHI_ISSUE_C_ENABLE)
            DoNotDataPull,
#endif
            RetToSrc,
            TraceTag,
#ifdef CHI_ISSUE_EB_ENABLE
            MPAM,
#endif
        };

        struct SNPFlitBuildability {
            bool QoS            = false;
            bool SrcID          = false;
            bool TxnID          = false;
            bool FwdNID         = false;
            bool FwdTxnID       = false;
            bool StashLPID      = false;
            bool StashLPIDValid = false;
            bool VMIDExt        = false;
            bool Opcode         = false;
            bool Addr           = false;
            bool NS             = false;
            bool DoNotGoToSD    = false;
#if defined(CHI_ISSUE_B_ENABLE) || defined(CHI_ISSUE_C_ENABLE)
            bool DoNotDataPull  = false;
#endif
            bool RetToSrc       = false;
            bool TraceTag       = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool MPAM           = false;
#endif
        };

        template<SNPFlitBuildability able>
        concept is_buildable = able.QoS
                            && able.SrcID
                            && able.TxnID
                            && able.FwdNID
                            && able.FwdTxnID
                            && able.StashLPID
                            && able.StashLPIDValid
                            && able.VMIDExt
                            && able.Opcode
                            && able.Addr
                            && able.NS
                            && able.DoNotGoToSD
#if defined(CHI_ISSUE_B_ENABLE) || defined(CHI_ISSUE_C_ENABLE)
                            && able.DoNotDataPull
#endif
                            && able.RetToSrc
                            && able.TraceTag
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.MPAM
#endif
                            ;

        template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config>
        consteval SNPFlitBuildability MakeSNPFlitBuildability() noexcept;

        template<SNPBuildTargetEnum T, SNPFlitBuildability prevAble, SNPFlitField field>
        consteval SNPFlitBuildability NextSNPFlitBuildability() noexcept;

        // patching common field writes
        template<SNPFlitConfigurationConcept config>
        constexpr typename SNP<config>::fwdtxnid_t PCFFwdTxnIDFromStashLPIDValid(typename SNP<config>::fwdtxnid_t, typename SNP<config>::stashlpidvalid_t) noexcept;

        template<SNPFlitConfigurationConcept config>
        constexpr typename SNP<config>::fwdtxnid_t PCFFwdTxnIDFromStashLPID(typename SNP<config>::fwdtxnid_t, typename SNP<config>::stashlpid_t) noexcept;

        template<SNPFlitConfigurationConcept config>
        constexpr typename SNP<config>::fwdtxnid_t PCFFwdTxnIDFromVMIDExt(typename SNP<config>::fwdtxnid_t, typename SNP<config>::vmidext_t) noexcept;

#if defined(CHI_ISSUE_B_ENABLE) || defined(CHI_ISSUE_C_ENABLE)
        template<SNPFlitConfigurationConcept config>
        constexpr typename SNP<config>::donotgotosd_t PCFDoNotGoToSDFromDoNotDataPull(typename SNP<config>::donotgotosd_t, typename SNP<config>::donotdatapull_t) noexcept;
#endif

        // patching common field reads
        template<SNPFlitConfigurationConcept config>
        constexpr typename SNP<config>::stashlpidvalid_t PCFFwdTxnIDToStashLPIDValid(typename SNP<config>::fwdtxnid_t) noexcept;

        template<SNPFlitConfigurationConcept config>
        constexpr typename SNP<config>::stashlpid_t PCFFwdTxnIDToStashLPID(typename SNP<config>::fwdtxnid_t) noexcept;

        template<SNPFlitConfigurationConcept config>
        constexpr typename SNP<config>::vmidext_t PCFFwdTxnIDToVMIDExt(typename SNP<config>::fwdtxnid_t) noexcept;

#if defined(CHI_ISSUE_B_ENABLE) || defined(CHI_ISSUE_C_ENABLE)
        template<SNPFlitConfigurationConcept config>
        constexpr typename SNP<config>::donotdatapull_t PCFDoNotGoToSDToDoNotDataPull(typename SNP<config>::donotgotosd_t) noexcept;
#endif
    }
/*
}
*/


// Implementation of: template MakeSNPFlitBuildability
namespace /*CHI::*/Flits::details {

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config>
    inline consteval SNPFlitBuildability MakeSNPFlitBuildability() noexcept
    {
#define _FIELD_ABLE(field) \
        able.field = IsFieldValueStableOrInapplicable<T->fields->field>();

#define _FIELD_ABLE_OPT(field) \
        if constexpr (SNP<config>::has##field) \
            _FIELD_ABLE(field) \
        else \
            able.field = true;

        SNPFlitBuildability able;
        _FIELD_ABLE(QoS             );
        _FIELD_ABLE(SrcID           );
        _FIELD_ABLE(TxnID           );
        _FIELD_ABLE(FwdNID          );
        _FIELD_ABLE(FwdTxnID        );
        _FIELD_ABLE(StashLPID       );
        _FIELD_ABLE(StashLPIDValid  );
        _FIELD_ABLE(VMIDExt         );
        _FIELD_ABLE(Opcode          );
        _FIELD_ABLE(Addr            );
        _FIELD_ABLE(NS              );
        _FIELD_ABLE(DoNotGoToSD     );
#if defined(CHI_ISSUE_B_ENABLE) || defined(CHI_ISSUE_C_ENABLE)
        _FIELD_ABLE(DoNotDataPull   );
#endif
        _FIELD_ABLE(RetToSrc        );
        _FIELD_ABLE(TraceTag        );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE_OPT(MPAM            );
#endif

        // Opcode always initialized
        able.Opcode = true;

        return able;

#undef _FIELD_ABLE
#undef _FIELD_ABLE_OPT
    }
}

// Implementation of: template NextSNPFlitBuildability
namespace /*CHI::*/Flits::details {

    template<SNPBuildTargetEnum T, SNPFlitBuildability prevAble, SNPFlitField field>
    inline consteval SNPFlitBuildability NextSNPFlitBuildability() noexcept
    {
        SNPFlitBuildability nextAble = prevAble;
        if constexpr (field == SNPFlitField::QoS            ) nextAble.QoS              = true;
        if constexpr (field == SNPFlitField::SrcID          ) nextAble.SrcID            = true;
        if constexpr (field == SNPFlitField::TxnID          ) nextAble.TxnID            = true;
        if constexpr (field == SNPFlitField::FwdNID         ) nextAble.FwdNID           = true;
        if constexpr (field == SNPFlitField::FwdTxnID       ) nextAble.FwdTxnID         = true;
        if constexpr (field == SNPFlitField::StashLPID      ) nextAble.StashLPID        = true;
        if constexpr (field == SNPFlitField::StashLPIDValid ) nextAble.StashLPIDValid   = true;
        if constexpr (field == SNPFlitField::VMIDExt        ) nextAble.VMIDExt          = true;
        if constexpr (field == SNPFlitField::Opcode         ) nextAble.Opcode           = true;
        if constexpr (field == SNPFlitField::Addr           ) nextAble.Addr             = true;
        if constexpr (field == SNPFlitField::NS             ) nextAble.NS               = true;
        if constexpr (field == SNPFlitField::DoNotGoToSD    ) nextAble.DoNotGoToSD      = true;
#if defined(CHI_ISSUE_B_ENABLE) || defined(CHI_ISSUE_C_ENABLE)
        if constexpr (field == SNPFlitField::DoNotDataPull  ) nextAble.DoNotDataPull    = true;
#endif
        if constexpr (field == SNPFlitField::RetToSrc       ) nextAble.RetToSrc         = true;
        if constexpr (field == SNPFlitField::TraceTag       ) nextAble.TraceTag         = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == SNPFlitField::MPAM           ) nextAble.MPAM             = true;
#endif
        return nextAble;
    }
}


// Implementation of common field patching write functions
namespace /*CHI::*/Flits::details {

    #define _PCF_WRITE(src, srcVal, dst, dstVal) \
        (  (uint64_t(dstVal) & ~(((0x1ULL << SNP<config>::src##_WIDTH) - 1) << (SNP<config>::src##_LSB - SNP<config>::dst##_LSB))) \
        | ((uint64_t(srcVal) & ((0x1ULL << SNP<config>::src##_WIDTH) - 1)) << (SNP<config>::src##_LSB - SNP<config>::dst##_LSB)))

    template<SNPFlitConfigurationConcept config>
    inline constexpr typename SNP<config>::fwdtxnid_t PCFFwdTxnIDFromStashLPIDValid(typename SNP<config>::fwdtxnid_t dst, typename SNP<config>::stashlpidvalid_t src) noexcept
    { return _PCF_WRITE(STASHLPIDVALID, src, FWDTXNID, dst); }

    template<SNPFlitConfigurationConcept config>
    inline constexpr typename SNP<config>::fwdtxnid_t PCFFwdTxnIDFromStashLPID(typename SNP<config>::fwdtxnid_t dst, typename SNP<config>::stashlpid_t src) noexcept
    { return _PCF_WRITE(STASHLPID, src, FWDTXNID, dst); }

    template<SNPFlitConfigurationConcept config>
    inline constexpr typename SNP<config>::fwdtxnid_t PCFFwdTxnIDFromVMIDExt(typename SNP<config>::fwdtxnid_t dst, typename SNP<config>::vmidext_t src) noexcept
    { return _PCF_WRITE(VMIDEXT, src, FWDTXNID, dst); }

#if defined(CHI_ISSUE_B_ENABLE) || defined(CHI_ISSUE_C_ENABLE)
    template<SNPFlitConfigurationConcept config>
    inline constexpr typename SNP<config>::donotgotosd_t PCFDoNotGoToSDFromDoNotDataPull(typename SNP<config>::donotgotosd_t dst, typename SNP<config>::donotdatapull_t src) noexcept
    { return _PCF_WRITE(DONOTDATAPULL, src, DONOTGOTOSD, dst); }
#endif

    #undef _PCF_WRITE
}

// Implementation of common field patching read functions
namespace /*CHI::*/Flits::details {

    #define _PCF_READ(dst, src, srcVal) \
    ((uint64_t(srcVal) & (((1ULL << SNP<config>::dst##_WIDTH) - 1) << (SNP<config>::dst##_LSB - SNP<config>::src##_LSB))) \
        >> (SNP<config>::dst##_LSB - SNP<config>::src##_LSB))

    template<SNPFlitConfigurationConcept config>
    inline constexpr typename SNP<config>::stashlpidvalid_t PCFFwdTxnIDToStashLPIDValid(typename SNP<config>::fwdtxnid_t src) noexcept
    { return _PCF_READ(STASHLPIDVALID, FWDTXNID, src); }

    template<SNPFlitConfigurationConcept config>
    inline constexpr typename SNP<config>::stashlpid_t PCFFwdTxnIDToStashLPID(typename SNP<config>::fwdtxnid_t src) noexcept
    { return _PCF_READ(STASHLPID, FWDTXNID, src); }

    template<SNPFlitConfigurationConcept config>
    inline constexpr typename SNP<config>::vmidext_t PCFFwdTxnIDToVMIDExt(typename SNP<config>::fwdtxnid_t src) noexcept
    { return _PCF_READ(VMIDEXT, FWDTXNID, src); }

#if defined(CHI_ISSUE_B_ENABLE) || defined(CHI_ISSUE_C_ENABLE)
    template<SNPFlitConfigurationConcept config>
    inline constexpr typename SNP<config>::donotdatapull_t PCFDoNotGoToSDToDoNotDataPull(typename SNP<config>::donotgotosd_t src) noexcept
    { return _PCF_READ(DONOTDATAPULL, DONOTGOTOSD, src); }
#endif

    #undef _PCF_READ
}


#endif // __CHI__CHI_UTIL_FLIT_BUILDER_*__SNP_DETAILS