//#pragma once

//#ifndef __CHI__CHI_UTIL_FLIT_BUILDER
//#define __CHI__CHI_UTIL_FLIT_BUILDER

#ifndef CHI_UTIL_FLIT_BUILDER__STANDALONE
#   define CHI_ISSUE_EB_ENABLE
#   include "chi_util_flit_builder_header.hpp"          // IWYU pragma: keep
#   include "../xact/chi_xact_field_eb.hpp"             // IWYU pragma: keep
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_EB
#endif


/*
namespace CHI {
*/
    namespace Flits {

        // enumerations
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

        namespace REQBuildTarget {
            inline constexpr REQBuildTargetEnumBack     ReqLCrdReturn   ("ReqLCrdReturn", Opcodes::REQ::ReqLCrdReturn, &Xact::RequestFieldMappings::ReqLCrdReturn);
            // TODO
        }


        // concepts
        template<Xact::FieldConvention conv>
        concept is_field_applicable = Xact::FieldTrait::IsApplicable(conv);

        
        // constevals
        template<Xact::FieldConvention conv>
        consteval uint64_t GetDefaultFieldValue() noexcept;

        template<Xact::FieldConvention conv>
        consteval bool HasDefaultFieldValue() noexcept;

        template<Xact::FieldConvention conv>
        consteval bool IsFieldValueStableOrInapplicable() noexcept;

        template<REQBuildTargetEnum T>
        consteval MemAttr GetDefaultMemAttrValue() noexcept;


        // REQ builders
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

        template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able = MakeREQFlitBuildability<T, config>()>
        class REQFlitBuilder
        {
        protected:
            REQ<config>     flit    = { 0 };

        public:
            constexpr REQFlitBuilder() noexcept;

            template<REQFlitBuildability nextAble>
            constexpr REQFlitBuilder(const REQFlitBuilder<T, config, nextAble>&) noexcept;

        public:
            constexpr REQ<config>::qos_t                QoS() const noexcept requires is_field_applicable<T->fields->QoS>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::QoS>()>
                                                        QoS(REQ<config>::qos_t qos) const noexcept requires is_field_applicable<T->fields->QoS>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::QoS>()>&
                                                        SetQoS(REQ<config>::qos_t qos) noexcept requires is_field_applicable<T->fields->QoS>;

            // TODO
            
            consteval REQ<config>                       Eval() const noexcept requires is_buildable<able>;
            constexpr REQ<config>                       Build() const noexcept requires is_buildable<able>;
        };
        
        template<REQBuildTargetEnum T, REQFlitConfigurationConcept config>
        consteval REQFlitBuilder<T, config, MakeREQFlitBuildability<T, config>()> MakeREQFlitBuilder() noexcept;

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
        consteval typename REQ<config>::stashnid_t PCFReturnNIDToStashNID(typename REQ<config>::returnnid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        consteval typename REQ<config>::slcrephint_t PCFReturnNIDToSLCRepHint(typename REQ<config>::returnnid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        consteval typename REQ<config>::endian_t PCFStashNIDValidToEndian(typename REQ<config>::stashnidvalid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        consteval typename REQ<config>::deep_t PCFStashNIDValidToDeep(typename REQ<config>::stashnidvalid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        consteval typename REQ<config>::stashlpidvalid_t PCFReturnTxnIDToStashLPIDValid(typename REQ<config>::returntxnid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        consteval typename REQ<config>::stashlpid_t PCFReturnTxnIDToStashLPID(typename REQ<config>::returntxnid_t) noexcept;

#ifdef CHI_ISSUE_EB_ENABLE
        template<REQFlitConfigurationConcept config>
        consteval typename REQ<config>::dodwt_t PCFSnpAttrToDoDWT(typename REQ<config>::snpattr_t) noexcept;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
        template<REQFlitConfigurationConcept config>
        consteval typename REQ<config>::stashgroupid_t PCFPGroupIDToStashGroupID(typename REQ<config>::pgroupid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        consteval typename REQ<config>::taggroupid_t PCFPGroupIDToTagGroupID(typename REQ<config>::pgroupid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        consteval typename REQ<config>::lpid_t PCFPGroupIDToLPID(typename REQ<config>::pgroupid_t) noexcept;
#endif

        template<REQFlitConfigurationConcept config>
        consteval typename REQ<config>::snoopme_t PCFExclToSnoopMe(typename REQ<config>::excl_t) noexcept;
    }

/*
}
*/


// Implementation of constevals
namespace /*CHI::*/Flits {

    template<Xact::FieldConvention conv>
    inline consteval uint64_t GetDefaultFieldValue() noexcept
    {
        if constexpr (conv == Xact::FieldConvention::A0)
            return 0;
        else if constexpr (conv == Xact::FieldConvention::A1)
            return 1;
        else if constexpr (conv == Xact::FieldConvention::I0)
            return 0;
        else if constexpr (conv == Xact::FieldConvention::X)
            return 0; // can take any value here, leave it 0 as default
        else if constexpr (conv == Xact::FieldConvention::Y)
            return 0; // applicable, leave to 0 as default
        else if constexpr (conv == Xact::FieldConvention::B8)
            return Sizes::B8;
        else if constexpr (conv == Xact::FieldConvention::B64)
            return Sizes::B64;
        else if constexpr (conv == Xact::FieldConvention::S)
            return 0; // do not set default value
        else if constexpr (conv == Xact::FieldConvention::D)
            return 0; // associated with NS field, leave it 0 as default
        else
            return 0;
    }

    template<Xact::FieldConvention conv>
    inline consteval bool HasDefaultFieldValue() noexcept
    {
        if constexpr (conv == Xact::FieldConvention::X)
            return false;
        else if constexpr (conv == Xact::FieldConvention::Y)
            return false;
        else if constexpr (conv == Xact::FieldConvention::S)
            return false;
        else
            return true;
    }

    template<Xact::FieldConvention conv>
    inline consteval bool IsFieldValueStableOrInapplicable() noexcept
    {
        if constexpr (conv == Xact::FieldConvention::A0)
            return true;
        else if constexpr (conv == Xact::FieldConvention::A1)
            return true;
        else if constexpr (conv == Xact::FieldConvention::I0)
            return true;
        else if constexpr (conv == Xact::FieldConvention::X)
            return true;
        else if constexpr (conv == Xact::FieldConvention::B8)
            return true;
        else if constexpr (conv == Xact::FieldConvention::B64)
            return true;
        else if constexpr (conv == Xact::FieldConvention::S)
            return true;
        else if constexpr (conv == Xact::FieldConvention::D)
            return true;
        else
            return false;
    }

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


// Implementation of common field patching functions
namespace /*CHI::*/Flits {

    #define _PCF(src, srcVal, dst, dstVal) \
        (  (uint64_t(dstVal) & ~(((0x1ULL << REQ<config>::src##_WIDTH) - 1) << (REQ<config>::src##_LSB - REQ<config>::dst##_LSB))) \
        | ((uint64_t(srcVal) & ((0x1ULL << REQ<config>::src##_WIDTH) - 1)) << (REQ<config>::src##_LSB - REQ<config>::dst##_LSB)))

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::returnnid_t PCFReturnNIDFromStashNID(typename REQ<config>::returnnid_t dst, typename REQ<config>::stashnid_t src) noexcept
    { return _PCF(STASHNID, src, RETURNNID, dst); }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::returnnid_t PCFReturnNIDFromSLCRepHint(typename REQ<config>::returnnid_t dst, typename REQ<config>::slcrephint_t src) noexcept
    { return _PCF(SLCREPHINT, src, RETURNNID, dst); }
#endif

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::stashnidvalid_t PCFStashNIDValidFromEndian(typename REQ<config>::stashnidvalid_t dst, typename REQ<config>::endian_t src) noexcept
    { return _PCF(ENDIAN, src, STASHNIDVALID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::stashnidvalid_t PCFStashNIDValidFromDeep(typename REQ<config>::stashnidvalid_t dst, typename REQ<config>::deep_t src) noexcept
    { return _PCF(DEEP, src, STASHNIDVALID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::returntxnid_t PCFReturnTxnIDFromStashLPIDValid(typename REQ<config>::returntxnid_t dst, typename REQ<config>::stashlpidvalid_t src) noexcept
    { return _PCF(STASHLPIDVALID, src, RETURNTXNID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::returntxnid_t PCFReturnTxnIDFromStashLPID(typename REQ<config>::returntxnid_t dst, typename REQ<config>::stashlpid_t src) noexcept
    { return _PCF(STASHLPID, src, RETURNTXNID, dst); }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::snpattr_t PCFSnpAttrFromDoDWT(typename REQ<config>::snpattr_t dst, typename REQ<config>::dodwt_t src) noexcept
    { return _PCF(DODWT, src, SNPATTR, dst); }
#endif

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromStashGroupID(typename REQ<config>::pgroupid_t dst, typename REQ<config>::stashgroupid_t src) noexcept
    { return _PCF(STASHGROUPID, src, PGROUPID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromTagGroupID(typename REQ<config>::pgroupid_t dst, typename REQ<config>::taggroupid_t src) noexcept
    { return _PCF(TAGGROUPID, src, PGROUPID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromLPID(typename REQ<config>::pgroupid_t dst, typename REQ<config>::lpid_t src) noexcept
    { return _PCF(LPID, src, PGROUPID, dst); }
#endif

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::excl_t PCFExclFromSnoopMe(typename REQ<config>::excl_t dst, typename REQ<config>::snoopme_t src) noexcept
    { return _PCF(SNOOPME, src, EXCL, dst); }    

    #undef _PCF
}


// Implementation of: class REQFlitBuilder
namespace /*CHI::*/Flits {

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, able>::REQFlitBuilder() noexcept
    {
        REQ<config> flit = { 0 };

#define _INIT_IF(field) \
        if constexpr (HasDefaultFieldValue<T->fields->field>())

#define _FIELD_INIT(field) \
        _INIT_IF(field) flit.field() = GetDefaultFieldValue<T->fields->field>();

#define _FIELD_INIT_PCF(field, dst) \
        _INIT_IF(field) flit.dst() = PCF##dst##From##field <config>(flit.dst(), GetDefaultFieldValue<T->fields->field>());

#define _FIELD_INIT_OPT(field) \
        if constexpr (REQ<config>::has##field) \
            _INIT_IF(field) flit.field() = GetDefaultFieldValue<T->fields->field>();

        flit.Opcode() = T->opcode;

        _FIELD_INIT(QoS             );
        _FIELD_INIT(TgtID           );
        _FIELD_INIT(SrcID           );
        _FIELD_INIT(TxnID           );
        _FIELD_INIT(ReturnNID       );
    //  _FIELD_INIT(StashNID        );
        _FIELD_INIT_PCF(StashNID        , ReturnNID);
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT_PCF(SLCRepHint      , ReturnNID);
#endif
        _FIELD_INIT(StashNIDValid   );
        _FIELD_INIT_PCF(Endian          , StashNIDValid);
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT_PCF(Deep            , StashNIDValid);
#endif
        _FIELD_INIT(ReturnTxnID     );
        _FIELD_INIT_PCF(StashLPIDValid  , ReturnTxnID);
        _FIELD_INIT_PCF(StashLPID       , ReturnTxnID);
        _FIELD_INIT(Opcode          );
        _FIELD_INIT(Size            );
        _FIELD_INIT(Addr            );
        _FIELD_INIT(NS              );
        _FIELD_INIT(LikelyShared    );
        _FIELD_INIT(AllowRetry      );
        _FIELD_INIT(Order           );
        _FIELD_INIT(PCrdType        );
    //  _FIELD_INIT(MemAttr         );
        flit.MemAttr() = GetDefaultMemAttrValue<T>();
        _FIELD_INIT(SnpAttr         );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT_PCF(DoDWT           , SnpAttr);
#endif
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT(PGroupID        );
        _FIELD_INIT_PCF(StashGroupID    , PGroupID);
        _FIELD_INIT_PCF(TagGroupID      , PGroupID);
        _FIELD_INIT_PCF(LPID            , PGroupID);
#endif
#ifdef CHI_ISSUE_B_ENABLE
        _FIELD_INIT(LPID            );
#endif
        _FIELD_INIT(Excl            );
        _FIELD_INIT_PCF(SnoopMe         , Excl);
        _FIELD_INIT(ExpCompAck      );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT(TagOp           );
#endif
        _FIELD_INIT(TraceTag        );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT_OPT(MPAM);
#endif
        _FIELD_INIT_OPT(RSVDC);

#undef _INIT_IF
#undef _FIELD_INIT
#undef _FIELD_INIT_PCF
#undef _FIELD_INIT_OPT

        this->flit = flit;
    }

    // TODO

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQ<config> REQFlitBuilder<T, config, able>::Eval() const noexcept requires is_buildable<able>
    {
        return flit;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQ<config> REQFlitBuilder<T, config, able>::Build() const noexcept requires is_buildable<able>
    {
        return flit;
    }
}


// Implementation of: template MakeREQFlitBuildability
namespace /*CHI::*/Flits {

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
namespace /*CHI::*/Flits {

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


#endif

//#endif // __CHI__CHI_UTIL_FLIT_BUILDER
