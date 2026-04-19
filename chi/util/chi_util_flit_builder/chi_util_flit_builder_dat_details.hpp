//#pragma once

#include "includes.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_REQ_B__DAT_DETAILS)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_REQ_EB__DAT_DETAILS))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_REQ_B__DAT_DETAILS
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_REQ_EB__DAT_DETAILS
#endif


/*
namespace CHI {
*/
    namespace Flits {

        class DATBuildTargetEnumBack {
        public:
            const char*                     name;
            const int                       opcode;
            const Xact::DataFieldMapping    fields;

        public:
            inline constexpr DATBuildTargetEnumBack(const char* name, const int opcode, const Xact::DataFieldMapping fields) noexcept
            : name(name), opcode(opcode), fields(fields) {}

        public:
            inline constexpr operator int() const noexcept
            { return opcode; }

            inline constexpr operator const DATBuildTargetEnumBack*() const noexcept
            { return this; }

            inline constexpr bool operator==(const DATBuildTargetEnumBack& obj) const noexcept
            { return opcode == obj.opcode; }
            
            inline constexpr bool operator!=(const DATBuildTargetEnumBack& obj) const noexcept
            { return !(*this == obj); }
        };

        using DATBuildTargetEnum = const DATBuildTargetEnumBack*;
    }

    namespace Flits::details {

        enum class DATFlitField {
            QoS,
            TgtID,
            SrcID,
            TxnID,
            HomeNID,
            Opcode,
            RespErr,
            Resp,
            FwdState,
            DataPull,
            DataSource,
#ifdef CHI_ISSUE_EB_ENABLE
            CBusy,
#endif
            DBID,
            CCID,
            DataID,
#ifdef CHI_ISSUE_EB_ENABLE
            TagOp,
            Tag,
            TU,
#endif
            TraceTag,
            RSVDC,
            BE,
            Data,
            DataCheck,
            Poison
        };

        struct DATFlitBuildability {
            bool QoS            = false;
            bool TgtID          = false;
            bool SrcID          = false;
            bool TxnID          = false;
            bool HomeNID        = false;
            bool Opcode         = false;
            bool RespErr        = false;
            bool Resp           = false;
            bool FwdState       = false;
            bool DataPull       = false;
            bool DataSource     = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool CBusy          = false;
#endif
            bool DBID           = false;
            bool CCID           = false;
            bool DataID         = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool TagOp          = false;
            bool Tag            = false;
            bool TU             = false;
#endif
            bool TraceTag       = false;
            bool RSVDC          = false;
            bool BE             = false;
            bool Data           = false;
            bool DataCheck      = false;
            bool Poison         = false;
        };

        constexpr DATFlitBuildability always_buildable_dat = {
            .QoS        = true,
            .TgtID      = true,
            .SrcID      = true,
            .TxnID      = true,
            .HomeNID    = true,
            .Opcode     = true,
            .RespErr    = true,
            .Resp       = true,
            .FwdState   = true,
            .DataPull   = true,
            .DataSource = true,
#ifdef CHI_ISSUE_EB_ENABLE
            .CBusy      = true,
#endif
            .DBID       = true,
            .CCID       = true,
            .DataID     = true,
#ifdef CHI_ISSUE_EB_ENABLE
            .TagOp      = true,
            .Tag        = true,
            .TU         = true,
#endif
            .TraceTag   = true,
            .RSVDC      = true,
            .BE         = true,
            .Data       = true,
            .DataCheck  = true,
            .Poison     = true
        };

        template<DATFlitBuildability able>
        concept is_buildable = able.QoS
                            && able.TgtID
                            && able.SrcID
                            && able.TxnID
                            && able.HomeNID
                            && able.Opcode
                            && able.RespErr
                            && able.Resp
                            && able.FwdState
                            && able.DataPull
                            && able.DataSource
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.CBusy
#endif
                            && able.DBID
                            && able.CCID
                            && able.DataID
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.TagOp
                            && able.Tag
                            && able.TU
#endif
                            && able.TraceTag
                            && able.RSVDC
                            && able.BE
                            && able.Data
                            && able.DataCheck
                            && able.Poison
                            ;

        template<DATBuildTargetEnum T, DATFlitConfigurationConcept config>
        consteval DATFlitBuildability MakeDATFlitBuildability() noexcept;

        template<DATBuildTargetEnum T, DATFlitBuildability prevAble, DATFlitField field>
        consteval DATFlitBuildability NextDATFlitBuildability() noexcept;

        // patching common field writes
        template<DATFlitConfigurationConcept config>
        constexpr typename DAT<config>::datasource_t PCFDataSourceFromFwdState(typename DAT<config>::datasource_t, typename DAT<config>::fwdstate_t) noexcept;

        template<DATFlitConfigurationConcept config>
        constexpr typename DAT<config>::datasource_t PCFDataSourceFromDataPull(typename DAT<config>::datasource_t, typename DAT<config>::datapull_t) noexcept;

        // patching common field reads
        template<DATFlitConfigurationConcept config>
        constexpr typename DAT<config>::fwdstate_t PCFDataSourceToFwdState(typename DAT<config>::datasource_t src) noexcept;

        template<DATFlitConfigurationConcept config>
        constexpr typename DAT<config>::datapull_t PCFDataSourceToDataPull(typename DAT<config>::datasource_t src) noexcept;
    }
/*
}
*/


// Implementation of: template MakeDATFlitBuildability
namespace /*CHI::*/Flits::details {

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config>
    inline consteval DATFlitBuildability MakeDATFlitBuildability() noexcept
    {
#define _FIELD_ABLE(field) \
        able.field = IsFieldValueStableOrInapplicable<T->fields->field>();

#define _FIELD_ABLE_OPT(field) \
        if constexpr (DAT<config>::has##field) \
            _FIELD_ABLE(field) \
        else \
            able.field = true;

        DATFlitBuildability able;
        _FIELD_ABLE(QoS             );
        _FIELD_ABLE(TgtID           );
        _FIELD_ABLE(SrcID           );
        _FIELD_ABLE(TxnID           );
        _FIELD_ABLE(HomeNID         );
        _FIELD_ABLE(Opcode          );
        _FIELD_ABLE(RespErr         );
        _FIELD_ABLE(Resp            );
        _FIELD_ABLE(FwdState        );
        _FIELD_ABLE(DataPull        );
        _FIELD_ABLE(DataSource      );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(CBusy           );
#endif
        _FIELD_ABLE(DBID            );
        _FIELD_ABLE(CCID            );
        _FIELD_ABLE(DataID          );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(TagOp           );
        _FIELD_ABLE(Tag             );
        _FIELD_ABLE(TU              );
#endif
        _FIELD_ABLE(TraceTag        );
        _FIELD_ABLE_OPT(RSVDC           );
        _FIELD_ABLE(BE              );
        _FIELD_ABLE(Data            );
        _FIELD_ABLE_OPT(DataCheck       );
        _FIELD_ABLE_OPT(Poison          );

        // Opcode always initialized
        able.Opcode = true;

        return able;

#undef _FIELD_ABLE
#undef _FIELD_ABLE_OPT
    }
}

// Implementation of: template NextDATFlitBuildability
namespace /*CHI::*/Flits::details {

    template<DATBuildTargetEnum T, DATFlitBuildability prevAble, DATFlitField field>
    inline consteval DATFlitBuildability NextDATFlitBuildability() noexcept
    {
        DATFlitBuildability nextAble = prevAble;
        if constexpr (field == DATFlitField::QoS            ) nextAble.QoS          = true;
        if constexpr (field == DATFlitField::TgtID          ) nextAble.TgtID        = true;
        if constexpr (field == DATFlitField::SrcID          ) nextAble.SrcID        = true;
        if constexpr (field == DATFlitField::TxnID          ) nextAble.TxnID        = true;
        if constexpr (field == DATFlitField::HomeNID        ) nextAble.HomeNID      = true;
        if constexpr (field == DATFlitField::Opcode         ) nextAble.Opcode       = true;
        if constexpr (field == DATFlitField::RespErr        ) nextAble.RespErr      = true;
        if constexpr (field == DATFlitField::Resp           ) nextAble.Resp         = true;
        if constexpr (field == DATFlitField::FwdState       ) nextAble.FwdState     = true;
        if constexpr (field == DATFlitField::DataPull       ) nextAble.DataPull     = true;
        if constexpr (field == DATFlitField::DataSource     ) nextAble.DataSource   = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == DATFlitField::CBusy          ) nextAble.CBusy        = true;
#endif
        if constexpr (field == DATFlitField::DBID           ) nextAble.DBID         = true;
        if constexpr (field == DATFlitField::CCID           ) nextAble.CCID         = true;
        if constexpr (field == DATFlitField::DataID         ) nextAble.DataID       = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == DATFlitField::TagOp          ) nextAble.TagOp        = true;
        if constexpr (field == DATFlitField::Tag            ) nextAble.Tag          = true;
        if constexpr (field == DATFlitField::TU             ) nextAble.TU           = true;
#endif
        if constexpr (field == DATFlitField::TraceTag       ) nextAble.TraceTag     = true;
        if constexpr (field == DATFlitField::RSVDC          ) nextAble.RSVDC        = true;
        if constexpr (field == DATFlitField::BE             ) nextAble.BE           = true;
        if constexpr (field == DATFlitField::Data           ) nextAble.Data         = true;
        if constexpr (field == DATFlitField::DataCheck      ) nextAble.DataCheck    = true;
        if constexpr (field == DATFlitField::Poison         ) nextAble.Poison       = true;
        return nextAble;
    }
}


// Implementation of common field patching write functions
namespace /*CHI::*/Flits::details {

    #define _PCF_WRITE(src, srcVal, dst, dstVal) \
        (  (uint64_t(dstVal) & ~(((0x1ULL << DAT<config>::src##_WIDTH) - 1) << (DAT<config>::src##_LSB - DAT<config>::dst##_LSB))) \
        | ((uint64_t(srcVal) & ((0x1ULL << DAT<config>::src##_WIDTH) - 1)) << (DAT<config>::src##_LSB - DAT<config>::dst##_LSB)))

    template<DATFlitConfigurationConcept config>
    inline constexpr typename DAT<config>::datasource_t PCFDataSourceFromFwdState(typename DAT<config>::datasource_t dst, typename DAT<config>::fwdstate_t src) noexcept
    { return _PCF_WRITE(FWDSTATE, src, DATASOURCE, dst); }

    template<DATFlitConfigurationConcept config>
    inline constexpr typename DAT<config>::datasource_t PCFDataSourceFromDataPull(typename DAT<config>::datasource_t dst, typename DAT<config>::datapull_t src) noexcept
    { return _PCF_WRITE(DATAPULL, src, DATASOURCE, dst); }

    #undef _PCF_WRITE
}

// Implementation of common field patching read functions
namespace /*CHI::*/Flits::details {

    #define _PCF_READ(dst, src, srcVal) \
        ((uint64_t(srcVal) & (((1ULL << REQ<config>::dst##_WIDTH) - 1) << (REQ<config>::dst##_LSB - REQ<config>::src##_LSB))) \
            >> (REQ<config>::dst##_LSB - REQ<config>::src##_LSB))

    template<DATFlitConfigurationConcept config>
    inline constexpr typename DAT<config>::fwdstate_t PCFDataSourceToFwdState(typename DAT<config>::datasource_t src) noexcept
    { return _PCF_READ(FWDSTATE, DATASOURCE, src); }

    template<DATFlitConfigurationConcept config>
    inline constexpr typename DAT<config>::datapull_t PCFDataSourceToDataPull(typename DAT<config>::datasource_t src) noexcept
    { return _PCF_READ(DATAPULL, DATASOURCE, src); }

    #undef _PCF_READ
}


#endif // __CHI__CHI_UTIL_FLIT_BUILDER_REQ_*__DAT_DETAILS
