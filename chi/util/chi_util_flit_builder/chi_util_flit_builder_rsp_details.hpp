//#pragma once

#include "includes.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_B__RSP_DETAILS)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_EB__RSP_DETAILS))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_B__RSP_DETAILS
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_EB__RSP_DETAILS
#endif


/*
namespace CHI {
*/
    namespace Flits {

        class RSPBuildTargetEnumBack {
        public:
            const char*                         name;
            const int                           opcode;
            const Xact::ResponseFieldMapping    fields;

        public:
            inline constexpr RSPBuildTargetEnumBack(const char* name, const int opcode, const Xact::ResponseFieldMapping fields) noexcept
            : name(name), opcode(opcode), fields(fields) {}

        public:
            inline constexpr operator int() const noexcept
            { return opcode; }

            inline constexpr operator const RSPBuildTargetEnumBack*() const noexcept
            { return this; }

            inline constexpr bool operator==(const RSPBuildTargetEnumBack& obj) const noexcept
            { return opcode == obj.opcode; }
            
            inline constexpr bool operator!=(const RSPBuildTargetEnumBack& obj) const noexcept
            { return !(*this == obj); }
        };

        using RSPBuildTargetEnum = const RSPBuildTargetEnumBack*;
    }

    namespace Flits::details {

        enum class RSPFlitField {
            QoS,
            TgtID,
            SrcID,
            TxnID,
            Opcode,
            RespErr,
            Resp,
            FwdState,
            DataPull,
            CBusy,
            DBID,
            PGroupID,
            StashGroupID,
            TagGroupID,
            PCrdType,
            TagOp,
            TraceTag
        };

        struct RSPFlitBuildability {
            bool QoS            = false;
            bool TgtID          = false;
            bool SrcID          = false;
            bool TxnID          = false;
            bool Opcode         = false;
            bool RespErr        = false;
            bool Resp           = false;
            bool FwdState       = false;
            bool DataPull       = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool CBusy          = false;
#endif
            bool DBID           = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool PGroupID       = false;
            bool StashGroupID   = false;
            bool TagGroupID     = false;
#endif
            bool PCrdType       = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool TagOp          = false;
#endif
            bool TraceTag       = false;
        };

        constexpr RSPFlitBuildability always_buildable_rsp = {
            .QoS        = true,
            .TgtID      = true,
            .SrcID      = true,
            .TxnID      = true,
            .Opcode     = true,
            .RespErr    = true,
            .Resp       = true,
            .FwdState   = true,
            .DataPull   = true,
#ifdef CHI_ISSUE_EB_ENABLE
            .CBusy      = true,
#endif
            .DBID       = true,
#ifdef CHI_ISSUE_EB_ENABLE
            .PGroupID   = true,
            .StashGroupID = true,
            .TagGroupID = true,
#endif
            .PCrdType   = true,
#ifdef CHI_ISSUE_EB_ENABLE
            .TagOp      = true,
#endif
            .TraceTag   = true
        };

        template<RSPFlitBuildability able>
        concept is_buildable = able.QoS
                            && able.TgtID
                            && able.SrcID
                            && able.TxnID
                            && able.Opcode
                            && able.RespErr
                            && able.Resp
                            && able.FwdState
                            && able.DataPull
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.CBusy
#endif
                            && able.DBID
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.PGroupID
                            && able.StashGroupID
                            && able.TagGroupID
#endif
                            && able.PCrdType
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.TagOp
#endif
                            && able.TraceTag
                            ;

        template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config>
        consteval RSPFlitBuildability MakeRSPFlitBuildability() noexcept;

        template<RSPBuildTargetEnum T, RSPFlitBuildability prevAble, RSPFlitField field>
        consteval RSPFlitBuildability NextRSPFlitBuildability() noexcept;

        // patching common field writes
        template<SNPFlitConfigurationConcept config>
        constexpr typename RSP<config>::fwdstate_t PCFFwdStateFromDataPull(typename RSP<config>::fwdstate_t, typename RSP<config>::datapull_t) noexcept;

#ifdef CHI_ISSUE_EB_ENABLE
        template<RSPFlitConfigurationConcept config>
        constexpr typename RSP<config>::dbid_t PCFDBIDFromPGroupID(typename RSP<config>::dbid_t, typename RSP<config>::pgroupid_t) noexcept;

        template<RSPFlitConfigurationConcept config>
        constexpr typename RSP<config>::dbid_t PCFDBIDFromStashGroupID(typename RSP<config>::dbid_t, typename RSP<config>::stashgroupid_t) noexcept;

        template<RSPFlitConfigurationConcept config>
        constexpr typename RSP<config>::dbid_t PCFDBIDFromTagGroupID(typename RSP<config>::dbid_t, typename RSP<config>::taggroupid_t) noexcept;
#endif

        // patching common field reads
        template<RSPFlitConfigurationConcept config>
        constexpr typename RSP<config>::datapull_t PCFFwdStateToDataPull(typename RSP<config>::fwdstate_t src) noexcept;

#ifdef CHI_ISSUE_EB_ENABLE
        template<RSPFlitConfigurationConcept config>
        constexpr typename RSP<config>::pgroupid_t PCFDBIDToPGroupID(typename RSP<config>::dbid_t src) noexcept;

        template<RSPFlitConfigurationConcept config>
        constexpr typename RSP<config>::stashgroupid_t PCFDBIDToStashGroupID(typename RSP<config>::dbid_t src) noexcept;

        template<RSPFlitConfigurationConcept config>
        constexpr typename RSP<config>::taggroupid_t PCFDBIDToTagGroupID(typename RSP<config>::dbid_t src) noexcept;
#endif
    }

/*
}
*/


// Implementation of: template MakeRSPFlitBuildability
namespace /*CHI::*/Flits::details {

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config>
    inline consteval RSPFlitBuildability MakeRSPFlitBuildability() noexcept
    {
#define _FIELD_ABLE(field) \
        able.field = IsFieldValueStableOrInapplicable<T->fields->field>();

#define _FIELD_ABLE_OPT(field) \
        if constexpr (RSP<config>::has##field) \
            _FIELD_ABLE(field) \
        else \
            able.field = true;

        RSPFlitBuildability able;
        _FIELD_ABLE(QoS            );
        _FIELD_ABLE(TgtID          );
        _FIELD_ABLE(SrcID          );
        _FIELD_ABLE(TxnID          );
        _FIELD_ABLE(Opcode         );
        _FIELD_ABLE(RespErr        );
        _FIELD_ABLE(Resp           );
        _FIELD_ABLE(FwdState       );
        _FIELD_ABLE(DataPull       );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(CBusy          );
#endif
        _FIELD_ABLE(DBID           );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(PGroupID       );
        _FIELD_ABLE(StashGroupID   );
        _FIELD_ABLE(TagGroupID     );
#endif
        _FIELD_ABLE(PCrdType       );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(TagOp          );
#endif
        _FIELD_ABLE(TraceTag       );

        // Opcode always initialized
        able.Opcode = true;

        return able;

#undef _FIELD_ABLE
#undef _FIELD_ABLE_OPT
    }
}

// Implementation of: template NextRSPFlitBuildability
namespace /*CHI::*/Flits::details {

    template<RSPBuildTargetEnum T, RSPFlitBuildability prevAble, RSPFlitField field>
    inline consteval RSPFlitBuildability NextRSPFlitBuildability() noexcept
    {
        RSPFlitBuildability nextAble = prevAble;
        if constexpr (field == RSPFlitField::QoS            ) nextAble.QoS              = true;
        if constexpr (field == RSPFlitField::TgtID          ) nextAble.TgtID            = true;
        if constexpr (field == RSPFlitField::SrcID          ) nextAble.SrcID            = true;
        if constexpr (field == RSPFlitField::TxnID          ) nextAble.TxnID            = true;
        if constexpr (field == RSPFlitField::Opcode         ) nextAble.Opcode           = true;
        if constexpr (field == RSPFlitField::RespErr        ) nextAble.RespErr          = true;
        if constexpr (field == RSPFlitField::Resp           ) nextAble.Resp             = true;
        if constexpr (field == RSPFlitField::FwdState       ) nextAble.FwdState         = true;
        if constexpr (field == RSPFlitField::DataPull       ) nextAble.DataPull         = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == RSPFlitField::CBusy          ) nextAble.CBusy            = true;
#endif
        if constexpr (field == RSPFlitField::DBID           ) nextAble.DBID             = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == RSPFlitField::PGroupID       ) nextAble.PGroupID         = true;
        if constexpr (field == RSPFlitField::StashGroupID   ) nextAble.StashGroupID     = true;
        if constexpr (field == RSPFlitField::TagGroupID     ) nextAble.TagGroupID       = true;
#endif
        if constexpr (field == RSPFlitField::PCrdType       ) nextAble.PCrdType         = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == RSPFlitField::TagOp          ) nextAble.TagOp            = true;
#endif
        if constexpr (field == RSPFlitField::TraceTag       ) nextAble.TraceTag         = true;
        return nextAble;
    }
}


// Implementation of common field patching write functions
namespace /*CHI::*/Flits::details {

    #define _PCF_WRITE(src, srcVal, dst, dstVal) \
        (  (uint64_t(dstVal) & ~(((0x1ULL << RSP<config>::src##_WIDTH) - 1) << (RSP<config>::src##_LSB - RSP<config>::dst##_LSB))) \
        | ((uint64_t(srcVal) & ((0x1ULL << RSP<config>::src##_WIDTH) - 1)) << (RSP<config>::src##_LSB - RSP<config>::dst##_LSB)))

    template<RSPFlitConfigurationConcept config>
    inline constexpr typename RSP<config>::fwdstate_t PCFFwdStateFromDataPull(typename RSP<config>::fwdstate_t dst, typename RSP<config>::datapull_t src) noexcept
    { return _PCF_WRITE(DATAPULL, src, FWDSTATE, dst); }

#ifdef CHI_ISSUE_EB_ENABLE
    template<RSPFlitConfigurationConcept config>
    inline constexpr typename RSP<config>::dbid_t PCFDBIDFromPGroupID(typename RSP<config>::dbid_t dst, typename RSP<config>::pgroupid_t src) noexcept
    { return _PCF_WRITE(PGROUPID, src, DBID, dst); }

    template<RSPFlitConfigurationConcept config>
    inline constexpr typename RSP<config>::dbid_t PCFDBIDFromStashGroupID(typename RSP<config>::dbid_t dst, typename RSP<config>::stashgroupid_t src) noexcept
    { return _PCF_WRITE(STASHGROUPID, src, DBID, dst); }

    template<RSPFlitConfigurationConcept config>
    inline constexpr typename RSP<config>::dbid_t PCFDBIDFromTagGroupID(typename RSP<config>::dbid_t dst, typename RSP<config>::taggroupid_t src) noexcept
    { return _PCF_WRITE(TAGGROUPID, src, DBID, dst); }
#endif

    #undef _PCF_WRITE
}

// Implementation of common field patching read functions
namespace /*CHI::*/Flits::details {

    #define _PCF_READ(dst, src, srcVal) \
    ((uint64_t(srcVal) & (((1ULL << RSP<config>::dst##_WIDTH) - 1) << (RSP<config>::dst##_LSB - RSP<config>::src##_LSB))) \
        >> (RSP<config>::dst##_LSB - RSP<config>::src##_LSB))

    template<RSPFlitConfigurationConcept config>
    inline constexpr typename RSP<config>::datapull_t PCFFwdStateToDataPull(typename RSP<config>::fwdstate_t src) noexcept
    { return _PCF_READ(DATAPULL, FWDSTATE, src); }

#ifdef CHI_ISSUE_EB_ENABLE
    template<RSPFlitConfigurationConcept config>
    inline constexpr typename RSP<config>::pgroupid_t PCFDBIDToPGroupID(typename RSP<config>::dbid_t src) noexcept
    { return _PCF_READ(PGROUPID, DBID, src); }

    template<RSPFlitConfigurationConcept config>
    inline constexpr typename RSP<config>::stashgroupid_t PCFDBIDToStashGroupID(typename RSP<config>::dbid_t src) noexcept
    { return _PCF_READ(STASHGROUPID, DBID, src); }

    template<RSPFlitConfigurationConcept config>
    inline constexpr typename RSP<config>::taggroupid_t PCFDBIDToTagGroupID(typename RSP<config>::dbid_t src) noexcept
    { return _PCF_READ(TAGGROUPID, DBID, src); }
#endif

    #undef _PCF_READ
}


#endif // __CHI__CHI_UTIL_FLIT_BUILDER_*__RSP_DETAILS
