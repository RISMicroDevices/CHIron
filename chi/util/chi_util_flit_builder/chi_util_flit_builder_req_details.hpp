//#pragma once

#include "includes.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_B__REQ_DETAILS)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_EB__REQ_DETAILS))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_B__REQ_DETAILS
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_EB__REQ_DETAILS
#endif


/*
namespace CHI {
*/
    namespace Flits {

        class REQBuildTargetEnumBack {
        public:
            const char*                     name;
            const int                       opcode;
            const Xact::RequestFieldMapping fields;

        public:
            inline constexpr REQBuildTargetEnumBack(const char* name, const int opcode, const Xact::RequestFieldMapping fields) noexcept
            : name(name), opcode(opcode), fields(fields) {}

        public:
            inline constexpr operator int() const noexcept
            { return opcode; }

            inline constexpr operator const REQBuildTargetEnumBack*() const noexcept
            { return this; }

            inline constexpr bool operator==(const REQBuildTargetEnumBack& obj) const noexcept
            { return opcode == obj.opcode; }
            
            inline constexpr bool operator!=(const REQBuildTargetEnumBack& obj) const noexcept
            { return !(*this == obj); }
        };

        using REQBuildTargetEnum = const REQBuildTargetEnumBack*;

        // concepts
        template<REQBuildTargetEnum T>
        concept is_memattr_applicable = Xact::FieldTrait::IsApplicable(T->fields->EWA)
                                     || Xact::FieldTrait::IsApplicable(T->fields->Device)
                                     || Xact::FieldTrait::IsApplicable(T->fields->Cacheable)
                                     || Xact::FieldTrait::IsApplicable(T->fields->Allocate);

        // constevals
        template<REQBuildTargetEnum T>
        consteval MemAttr GetDefaultMemAttrValue() noexcept;
    }

    namespace Flits::details {

        enum class REQFlitField {
            QoS,
            TgtID,
            SrcID,
            TxnID,
            ReturnNID,
            StashNID,
#ifdef CHI_ISSUE_EB_ENABLE
            SLCRepHint,
#endif
            StashNIDValid,
            Endian,
#ifdef CHI_ISSUE_EB_ENABLE
            Deep,
#endif
            ReturnTxnID,
            StashLPIDValid,
            StashLPID,
            Opcode,
            Size,
            Addr,
            NS,
            LikelyShared,
            AllowRetry,
            Order,
            PCrdType,
            MemAttr,
            SnpAttr,
#ifdef CHI_ISSUE_EB_ENABLE
            DoDWT,
#endif
            LPID,
#ifdef CHI_ISSUE_EB_ENABLE
            PGroupID,
            StashGroupID,
            TagGroupID,
#endif
            Excl,
            SnoopMe,
            ExpCompAck,
#ifdef CHI_ISSUE_EB_ENABLE
            TagOp,
#endif
            TraceTag,
#ifdef CHI_ISSUE_EB_ENABLE
            MPAM,
#endif
            RSVDC
        };

        struct REQFlitBuildability {
            bool QoS            = false;
            bool TgtID          = false;
            bool SrcID          = false;
            bool TxnID          = false;
            bool ReturnNID      = false;
            bool StashNID       = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool SLCRepHint     = false;
#endif
            bool StashNIDValid  = false;
            bool Endian         = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool Deep           = false;
#endif
            bool ReturnTxnID    = false;
            bool StashLPIDValid = false;
            bool StashLPID      = false;
            bool Opcode         = false;
            bool Size           = false;
            bool Addr           = false;
            bool NS             = false;
            bool LikelyShared   = false;
            bool AllowRetry     = false;
            bool Order          = false;
            bool PCrdType       = false;
            bool MemAttr        = false;
            bool SnpAttr        = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool DoDWT          = false;
#endif
            bool LPID           = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool PGroupID       = false;
            bool StashGroupID   = false;
            bool TagGroupID     = false;
#endif
            bool Excl           = false;
            bool SnoopMe        = false;
            bool ExpCompAck     = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool TagOp          = false;
#endif
            bool TraceTag       = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool MPAM           = false;
#endif
            bool RSVDC          = false;
        };

        template<REQFlitBuildability able>
        concept is_buildable = able.QoS
                            && able.TgtID
                            && able.SrcID
                            && able.TxnID
                            && able.ReturnNID
                            && able.StashNID
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.SLCRepHint
#endif
                            && able.StashNIDValid
                            && able.Endian
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.Deep
#endif
                            && able.ReturnTxnID
                            && able.StashLPIDValid
                            && able.StashLPID
                            && able.Opcode
                            && able.Size
                            && able.Addr
                            && able.NS
                            && able.LikelyShared
                            && able.AllowRetry
                            && able.Order
                            && able.PCrdType
                            && able.MemAttr
                            && able.SnpAttr
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.DoDWT
#endif
                            && able.LPID
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.PGroupID
                            && able.StashGroupID
                            && able.TagGroupID
#endif
                            && able.Excl
                            && able.SnoopMe
                            && able.ExpCompAck
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.TagOp
#endif
                            && able.TraceTag
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.MPAM
#endif
                            && able.RSVDC
                            ;

        template<REQBuildTargetEnum T, REQFlitConfigurationConcept config>
        consteval REQFlitBuildability MakeREQFlitBuildability() noexcept;

        template<REQBuildTargetEnum T, REQFlitBuildability prevAble, REQFlitField field>
        consteval REQFlitBuildability NextREQFlitBuildability() noexcept;

        // patching common field writes
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::returnnid_t PCFReturnNIDFromStashNID(typename REQ<config>::returnnid_t, typename REQ<config>::stashnid_t) noexcept;

#ifdef CHI_ISSUE_EB_ENABLE
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::returnnid_t PCFReturnNIDFromSLCRepHint(typename REQ<config>::returnnid_t, typename REQ<config>::slcrephint_t) noexcept;
#endif

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::stashnidvalid_t PCFStashNIDValidFromEndian(typename REQ<config>::stashnidvalid_t, typename REQ<config>::endian_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::stashnidvalid_t PCFStashNIDValidFromDeep(typename REQ<config>::stashnidvalid_t, typename REQ<config>::deep_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::returntxnid_t PCFReturnTxnIDFromStashLPIDValid(typename REQ<config>::returntxnid_t, typename REQ<config>::stashlpidvalid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::returntxnid_t PCFReturnTxnIDFromStashLPID(typename REQ<config>::returntxnid_t, typename REQ<config>::stashlpid_t) noexcept;

#ifdef CHI_ISSUE_EB_ENABLE
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::snpattr_t PCFSnpAttrFromDoDWT(typename REQ<config>::snpattr_t, typename REQ<config>::dodwt_t) noexcept;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromStashGroupID(typename REQ<config>::pgroupid_t, typename REQ<config>::stashgroupid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromTagGroupID(typename REQ<config>::pgroupid_t, typename REQ<config>::taggroupid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromLPID(typename REQ<config>::pgroupid_t, typename REQ<config>::lpid_t) noexcept;
#endif

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::excl_t PCFExclFromSnoopMe(typename REQ<config>::excl_t, typename REQ<config>::snoopme_t) noexcept;
    
        // patching common field reads
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::stashnid_t PCFReturnNIDToStashNID(typename REQ<config>::returnnid_t) noexcept;

#ifdef CHI_ISSUE_EB_ENABLE
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::slcrephint_t PCFReturnNIDToSLCRepHint(typename REQ<config>::returnnid_t) noexcept;
#endif

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::endian_t PCFStashNIDValidToEndian(typename REQ<config>::stashnidvalid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::deep_t PCFStashNIDValidToDeep(typename REQ<config>::stashnidvalid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::stashlpidvalid_t PCFReturnTxnIDToStashLPIDValid(typename REQ<config>::returntxnid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::stashlpid_t PCFReturnTxnIDToStashLPID(typename REQ<config>::returntxnid_t) noexcept;

#ifdef CHI_ISSUE_EB_ENABLE
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::dodwt_t PCFSnpAttrToDoDWT(typename REQ<config>::snpattr_t) noexcept;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::stashgroupid_t PCFPGroupIDToStashGroupID(typename REQ<config>::pgroupid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::taggroupid_t PCFPGroupIDToTagGroupID(typename REQ<config>::pgroupid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::lpid_t PCFPGroupIDToLPID(typename REQ<config>::pgroupid_t) noexcept;
#endif

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::snoopme_t PCFExclToSnoopMe(typename REQ<config>::excl_t) noexcept;
    }
/*
}
*/


// Implementation of constevals
namespace /*CHI::*/Flits {

    template<REQBuildTargetEnum T>
    inline consteval MemAttr GetDefaultMemAttrValue() noexcept
    {
        MemAttr attr = 0;
        if constexpr (GetDefaultFieldValue<T->fields->EWA>())       attr |= MemAttrs::EWA;
        if constexpr (GetDefaultFieldValue<T->fields->Device>())    attr |= MemAttrs::Device;
        if constexpr (GetDefaultFieldValue<T->fields->Cacheable>()) attr |= MemAttrs::Cacheable;
        if constexpr (GetDefaultFieldValue<T->fields->Allocate>())  attr |= MemAttrs::Allocate;
        return attr;
    }
}


// Implementation of: template MakeREQFlitBuildability
namespace /*CHI::*/Flits::details {

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config>
    inline consteval REQFlitBuildability MakeREQFlitBuildability() noexcept
    {
#define _FIELD_ABLE(field) \
        able.field = IsFieldValueStableOrInapplicable<T->fields->field>();

#define _FIELD_ABLE_OPT(field) \
        if constexpr (REQ<config>::has##field) \
            _FIELD_ABLE(field) \
        else \
            able.field = true;

        REQFlitBuildability able;
        _FIELD_ABLE(QoS             );
        _FIELD_ABLE(TgtID           );
        _FIELD_ABLE(SrcID           );
        _FIELD_ABLE(TxnID           );
        _FIELD_ABLE(ReturnNID       );
        _FIELD_ABLE(StashNID        );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(SLCRepHint      );
#endif
        _FIELD_ABLE(StashNIDValid   );
        _FIELD_ABLE(Endian          );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(Deep            );
#endif
        _FIELD_ABLE(ReturnTxnID     );
        _FIELD_ABLE(StashLPIDValid  );
        _FIELD_ABLE(StashLPID       );
        _FIELD_ABLE(Opcode          );
        _FIELD_ABLE(Size            );
        _FIELD_ABLE(Addr            );
        _FIELD_ABLE(NS              );
        _FIELD_ABLE(LikelyShared    );
        _FIELD_ABLE(AllowRetry      );
        _FIELD_ABLE(Order           );
        _FIELD_ABLE(PCrdType        );
    //  _FIELD_ABLE(MemAttr         );
        able.MemAttr = IsFieldValueStableOrInapplicable<T->fields->Device>()
                    && IsFieldValueStableOrInapplicable<T->fields->Cacheable>()
                    && IsFieldValueStableOrInapplicable<T->fields->EWA>()
                    && IsFieldValueStableOrInapplicable<T->fields->Allocate>();
        _FIELD_ABLE(SnpAttr         );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(DoDWT           );
#endif
        _FIELD_ABLE(LPID            );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(PGroupID        );
        _FIELD_ABLE(StashGroupID    );
        _FIELD_ABLE(TagGroupID      );
#endif
        _FIELD_ABLE(Excl            );
        _FIELD_ABLE(SnoopMe         );
        _FIELD_ABLE(ExpCompAck      );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(TagOp           );
#endif
        _FIELD_ABLE(TraceTag        );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE_OPT(MPAM            );
#endif
        
        _FIELD_ABLE_OPT(RSVDC           );

        // Opcode always initialized
        able.Opcode = true;

        return able;

#undef _FIELD_ABLE
#undef _FIELD_ABLE_OPT
    }
}

// Implementation of: template NextREQFlitBuildability
namespace /*CHI::*/Flits::details {

    template<REQBuildTargetEnum T, REQFlitBuildability prevAble, REQFlitField field>
    inline consteval REQFlitBuildability NextREQFlitBuildability() noexcept
    {
        REQFlitBuildability nextAble = prevAble;
        if constexpr (field == REQFlitField::QoS            ) nextAble.QoS              = true;
        if constexpr (field == REQFlitField::TgtID          ) nextAble.TgtID            = true;
        if constexpr (field == REQFlitField::SrcID          ) nextAble.SrcID            = true;
        if constexpr (field == REQFlitField::TxnID          ) nextAble.TxnID            = true;
        if constexpr (field == REQFlitField::ReturnNID      ) nextAble.ReturnNID        = true;
        if constexpr (field == REQFlitField::StashNID       ) nextAble.StashNID         = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == REQFlitField::SLCRepHint     ) nextAble.SLCRepHint       = true;
#endif
        if constexpr (field == REQFlitField::StashNIDValid  ) nextAble.StashNIDValid    = true;
        if constexpr (field == REQFlitField::Endian         ) nextAble.Endian           = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == REQFlitField::Deep           ) nextAble.Deep             = true;
#endif
        if constexpr (field == REQFlitField::ReturnTxnID    ) nextAble.ReturnTxnID      = true;
        if constexpr (field == REQFlitField::StashLPIDValid ) nextAble.StashLPIDValid   = true;
        if constexpr (field == REQFlitField::StashLPID      ) nextAble.StashLPID        = true;
        if constexpr (field == REQFlitField::Opcode         ) nextAble.Opcode           = true;
        if constexpr (field == REQFlitField::Size           ) nextAble.Size             = true;
        if constexpr (field == REQFlitField::Addr           ) nextAble.Addr             = true;
        if constexpr (field == REQFlitField::NS             ) nextAble.NS               = true;
        if constexpr (field == REQFlitField::LikelyShared   ) nextAble.LikelyShared     = true;
        if constexpr (field == REQFlitField::AllowRetry     ) nextAble.AllowRetry       = true;
        if constexpr (field == REQFlitField::Order          ) nextAble.Order            = true;
        if constexpr (field == REQFlitField::PCrdType       ) nextAble.PCrdType         = true;
        if constexpr (field == REQFlitField::MemAttr        ) nextAble.MemAttr          = true;
        if constexpr (field == REQFlitField::SnpAttr        ) nextAble.SnpAttr          = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == REQFlitField::DoDWT          ) nextAble.DoDWT            = true;
#endif
        if constexpr (field == REQFlitField::LPID           ) nextAble.LPID             = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == REQFlitField::PGroupID       ) nextAble.PGroupID         = true;
        if constexpr (field == REQFlitField::StashGroupID   ) nextAble.StashGroupID     = true;
        if constexpr (field == REQFlitField::TagGroupID     ) nextAble.TagGroupID       = true;
#endif
        if constexpr (field == REQFlitField::Excl           ) nextAble.Excl             = true;
        if constexpr (field == REQFlitField::SnoopMe        ) nextAble.SnoopMe          = true;
        if constexpr (field == REQFlitField::ExpCompAck     ) nextAble.ExpCompAck       = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == REQFlitField::TagOp          ) nextAble.TagOp            = true;
#endif
        if constexpr (field == REQFlitField::TraceTag       ) nextAble.TraceTag         = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == REQFlitField::MPAM           ) nextAble.MPAM             = true;
#endif
        if constexpr (field == REQFlitField::RSVDC          ) nextAble.RSVDC            = true;
        return nextAble;
    }
}


// Implementation of common field patching write functions
namespace /*CHI::*/Flits::details {

    #define _PCF_WRITE(src, srcVal, dst, dstVal) \
        (  (uint64_t(dstVal) & ~(((0x1ULL << REQ<config>::src##_WIDTH) - 1) << (REQ<config>::src##_LSB - REQ<config>::dst##_LSB))) \
        | ((uint64_t(srcVal) & ((0x1ULL << REQ<config>::src##_WIDTH) - 1)) << (REQ<config>::src##_LSB - REQ<config>::dst##_LSB)))

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::returnnid_t PCFReturnNIDFromStashNID(typename REQ<config>::returnnid_t dst, typename REQ<config>::stashnid_t src) noexcept
    { return _PCF_WRITE(STASHNID, src, RETURNNID, dst); }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::returnnid_t PCFReturnNIDFromSLCRepHint(typename REQ<config>::returnnid_t dst, typename REQ<config>::slcrephint_t src) noexcept
    { return _PCF_WRITE(SLCREPHINT, src, RETURNNID, dst); }
#endif

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::stashnidvalid_t PCFStashNIDValidFromEndian(typename REQ<config>::stashnidvalid_t dst, typename REQ<config>::endian_t src) noexcept
    { return _PCF_WRITE(ENDIAN, src, STASHNIDVALID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::stashnidvalid_t PCFStashNIDValidFromDeep(typename REQ<config>::stashnidvalid_t dst, typename REQ<config>::deep_t src) noexcept
    { return _PCF_WRITE(DEEP, src, STASHNIDVALID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::returntxnid_t PCFReturnTxnIDFromStashLPIDValid(typename REQ<config>::returntxnid_t dst, typename REQ<config>::stashlpidvalid_t src) noexcept
    { return _PCF_WRITE(STASHLPIDVALID, src, RETURNTXNID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::returntxnid_t PCFReturnTxnIDFromStashLPID(typename REQ<config>::returntxnid_t dst, typename REQ<config>::stashlpid_t src) noexcept
    { return _PCF_WRITE(STASHLPID, src, RETURNTXNID, dst); }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::snpattr_t PCFSnpAttrFromDoDWT(typename REQ<config>::snpattr_t dst, typename REQ<config>::dodwt_t src) noexcept
    { return _PCF_WRITE(DODWT, src, SNPATTR, dst); }
#endif

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromStashGroupID(typename REQ<config>::pgroupid_t dst, typename REQ<config>::stashgroupid_t src) noexcept
    { return _PCF_WRITE(STASHGROUPID, src, PGROUPID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromTagGroupID(typename REQ<config>::pgroupid_t dst, typename REQ<config>::taggroupid_t src) noexcept
    { return _PCF_WRITE(TAGGROUPID, src, PGROUPID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromLPID(typename REQ<config>::pgroupid_t dst, typename REQ<config>::lpid_t src) noexcept
    { return _PCF_WRITE(LPID, src, PGROUPID, dst); }
#endif

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::excl_t PCFExclFromSnoopMe(typename REQ<config>::excl_t dst, typename REQ<config>::snoopme_t src) noexcept
    { return _PCF_WRITE(SNOOPME, src, EXCL, dst); }    

    #undef _PCF_WRITE
}

// Implementation of common field patching read functions
namespace /*CHI::*/Flits::details {

#define _PCF_READ(dst, src, srcVal) \
    ((uint64_t(srcVal) & (((1ULL << REQ<config>::dst##_WIDTH) - 1) << (REQ<config>::dst##_LSB - REQ<config>::src##_LSB))) \
        >> (REQ<config>::dst##_LSB - REQ<config>::src##_LSB))

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::stashnid_t PCFReturnNIDToStashNID(typename REQ<config>::returnnid_t src) noexcept
    { return _PCF_READ(STASHNID, RETURNNID, src); }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::slcrephint_t PCFReturnNIDToSLCRepHint(typename REQ<config>::returnnid_t src) noexcept
    { return _PCF_READ(SLCREPHINT, RETURNNID, src); }
#endif

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::endian_t PCFStashNIDValidToEndian(typename REQ<config>::stashnidvalid_t src) noexcept
    { return _PCF_READ(ENDIAN, STASHNIDVALID, src); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::deep_t PCFStashNIDValidToDeep(typename REQ<config>::stashnidvalid_t src) noexcept
    { return _PCF_READ(DEEP, STASHNIDVALID, src); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::stashlpidvalid_t PCFReturnTxnIDToStashLPIDValid(typename REQ<config>::returntxnid_t src) noexcept
    { return _PCF_READ(STASHLPIDVALID, RETURNTXNID, src); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::stashlpid_t PCFReturnTxnIDToStashLPID(typename REQ<config>::returntxnid_t src) noexcept
    { return _PCF_READ(STASHLPID, RETURNTXNID, src); }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::dodwt_t PCFSnpAttrToDoDWT(typename REQ<config>::snpattr_t src) noexcept
    { return _PCF_READ(DODWT, SNPATTR, src); }
#endif

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::stashgroupid_t PCFPGroupIDToStashGroupID(typename REQ<config>::pgroupid_t src) noexcept
    { return _PCF_READ(STASHGROUPID, PGROUPID, src); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::taggroupid_t PCFPGroupIDToTagGroupID(typename REQ<config>::pgroupid_t src) noexcept
    { return _PCF_READ(TAGGROUPID, PGROUPID, src); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::lpid_t PCFPGroupIDToLPID(typename REQ<config>::pgroupid_t src) noexcept
    { return _PCF_READ(LPID, PGROUPID, src); }
#endif

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::snoopme_t PCFExclToSnoopMe(typename REQ<config>::excl_t src) noexcept
    { return _PCF_READ(SNOOPME, EXCL, src); }

#undef _PCF_READ
}


#endif // __CHI__CHI_UTIL_FLIT_BUILDER_*__REQ_DETAILS
